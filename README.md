# hyper 项目分析说明

> Official Forum / 官方论坛: https://discord.gg/qslab

## 说明

本文档面向受控实验环境中的反作弊、内核安全、启动链安全分析人员。内容目标是把这个仓库的工程结构、启动链行为、Hyper-V 附着方式、hypercall 协议、用户态控制端能力、隐匿/规避相关实现和可观测检测面说明白。

`hyper` 不是普通应用，也不是常规 Windows 驱动。它是一套围绕 Windows UEFI 启动链和 Hyper-V VM-exit 路径构建的多阶段研究项目。它试图在 Windows 启动过程中把一个自定义运行时接入 Hyper-V 的 VM-exit handler，然后让用户态 `client.exe` 通过 `CPUID` 指令触发自定义 hypercall，从而获得跨地址空间读写、SLAT/EPT/NPT hook、隐藏页、CR3 缓存、PFN 查询等能力。

请只在授权、隔离、可恢复的测试环境中分析。本文档不把它当作可部署工具来写，而是把它当作反作弊对抗样本和内核安全研究样本来拆解。

## 一句话概括

这个项目的核心链路是：

```text
UEFI bootloader
  -> 恢复并链式启动原始 bootmgfw.efi
  -> hook bootmgfw 的 PE 加载路径
  -> 捕获 winload.efi
  -> hook winload 的 PE 加载路径和内存映射构建路径
  -> 捕获 ntoskrnl.exe 基址和 hvloader
  -> hook hvloader 启动 Hyper-V 的路径
  -> 在 Hyper-V text 区扫描 VM-exit handler 和 code cave
  -> 把 hyperv-attachment.dll 里的 VM-exit detour 接进去
  -> Windows 启动后 client.exe 通过 CPUID 进入自定义 hypercall
```

从反作弊视角看，它属于“启动前注入 + Hyper-V VM-exit 劫持 + 二级页表视图切换 + 用户态控制器”的组合。

## 解决方案组成

`hyper.sln` 包含 4 个 Visual Studio 工程：

| 工程 | 路径 | 主要输出 | 语言 | 作用 |
|---|---|---|---|---|
| `bootloader` | `src/bootloader` | `.efi` | C | UEFI 阶段启动链附着器 |
| `hypervisor` | `src/hypervisor` | `.dll` | C++20 / MASM | 被加载进 Hyper-V 上下文的 VM-exit detour 运行时 |
| `client` | `src/client` | `.exe` | C++ 最新标准 / MASM | Windows 用户态交互控制端 |
| `basic-test` | `tests/basic-test` | `.dll` | C++ | DLL 加载实验样例 |

仓库根目录：

```text
.
|-- hyper.sln
|-- README.md
|-- docs/
|-- src/
|   |-- bootloader/
|   |-- hypervisor/
|   |-- client/
|   `-- common/
|-- tests/
`-- external/
```

## 总体架构

```text
                        +----------------------+
                        |      src/common      |
                        | hypercall + structs  |
                        +----------+-----------+
                                   |
        +--------------------------+--------------------------+
        |                                                     |
+-------v--------+                                  +---------v--------+
|  bootloader    |                                  |     client       |
| UEFI stage     |                                  | usermode console |
+-------+--------+                                  +---------+--------+
        |                                                     |
        | loads / relocates                                   | CPUID
        |                                                     |
+-------v-----------------------------------------------------v--------+
|                         hypervisor                                   |
|        Hyper-V VM-exit detour + SLAT/EPT/NPT + hypercall server      |
+---------------------------------------------------------------------+
```

职责分离：

- `bootloader`: 负责“什么时候接入”和“如何把 attachment 带进 Hyper-V”。
- `hypervisor`: 负责“接到 VM-exit 后做什么”。
- `client`: 负责“Windows 已启动后如何发命令”。
- `common`: 负责“client 和 hypervisor 之间如何约定协议”。

## 启动链细节

### 1. UEFI 入口

入口在 `src/bootloader/src/main.c` 的 `UefiMain`。

当前主流程：

1. 打印 `=== Hyper UEFI Boot Manager ===`。
2. 恢复原始 `bootmgfw.efi`。
3. 设置 Hyper-V attachment。
4. 加载并启动原始 `bootmgfw.efi`，同时对其放置 hook。

相关路径常量：

| 常量 | 值 | 含义 |
|---|---|---|
| `d_bootmgfw_path` | `\efi\microsoft\boot\bootmgfw.efi` | Windows Boot Manager 路径 |
| `d_path_original_bootmgfw` | `\efi\microsoft\boot\bootmgfw.original.efi` | 原始 bootmgfw 备份路径 |
| `d_hyperv_attachment_path` | `\efi\microsoft\boot\hyperv-attachment.dll` | 被加载的 hypervisor attachment 文件 |

### 2. bootmgfw 阶段

代码在 `src/bootloader/src/bootmgfw/bootmgfw.c`。

关键函数：

| 函数 | 作用 |
|---|---|
| `bootmgfw_restore_original_file` | 从 `bootmgfw.original.efi` 读回原始 boot manager，写回 `bootmgfw.efi`，然后删除 original 文件 |
| `bootmgfw_run_original_image` | 通过 UEFI `LoadImage` / `StartImage` 链式启动原始 bootmgfw |
| `bootmgfw_place_hooks` | 获取 loaded image 信息，给 bootmgfw 放置 PE 加载 hook |
| `bootmgfw_load_pe_image_detour` | 拦截 bootmgfw 加载 PE 的过程，发现 `winload.efi` 后调用 `winload_place_hooks` |
| `parse_uefi_boot_image_info` | 记录当前 UEFI bootloader 镜像的物理地址和大小，后续 hypervisor 会清理这段内存 |

分析要点：

- 它不是永久替换 bootmgfw 的完整实现，而是在执行前把原始 bootmgfw 恢复回来并链式启动。
- hook 目标通过模式扫描定位 `ImgpLoadPEImage` 相关调用点。
- `bootmgfw_run_original_image` 中调用了 `SetLegitimateBootmgfwInfo` 和 `CleanTpmBeforeChaining`，说明作者关注 TPM/TCG event log 中 bootmgfw 测量痕迹。

### 3. winload 阶段

代码在 `src/bootloader/src/winload/winload.c`。

关键函数：

| 函数 | 作用 |
|---|---|
| `winload_place_hooks` | 给 winload 放置两个主要 hook |
| `winload_place_load_pe_image_hook` | hook winload 的 PE 加载函数 |
| `winload_load_pe_image_detour` | 观察 winload 加载的 PE，识别 `ntoskrnl.exe` 和 `hvloader` |
| `winload_place_oslp_build_kernel_memory_map_hook` | hook `OslpBuildKernelMemoryMap` 签名 |
| `winload_oslp_build_kernel_memory_map_detour` | 修改 loader memory map 中 hypervisor heap 对应 descriptor 的类型 |
| `WriteDebugFile` | 向 EFI 分区根目录写 `debug.txt` |

具体行为：

- 发现 `ntoskrnl.exe` 时记录 `g_ntoskrnl_base_address`。
- 发现 `hvloader` 时调用 `hvloader_place_hooks`。
- 在 `OslpBuildKernelMemoryMap` hook 中遍历 `LoaderMemoryMap`，如果 descriptor 物理地址等于 `hyperv_attachment_heap_allocation_base`，则把 `MemoryType` 改为 `24`，即 `LoaderReserve`。
- 会把调试信息写入 `\debug.txt`，这是一个明显的磁盘侧观察点。

反作弊分析重点：

- 关注 winload 内存中 `ImgpLoadPEImage`、`OslpBuildKernelMemoryMap` 等位置是否存在 14 字节 trampoline。
- 关注 loader memory map 中异常的 reserved range，尤其是与 Hyper-V attachment heap 对应的连续页。
- 关注 EFI 分区根目录的 `debug.txt`。

### 4. hvloader 阶段

代码在 `src/bootloader/src/hvloader/hvloader.c`。

关键函数：

| 函数 | 作用 |
|---|---|
| `hvloader_place_hooks` | 通过签名找到 hvloader 启动 Hyper-V 的函数并 hook |
| `hvloader_launch_hv_detour` | 在 Hyper-V 启动前临时建立 identity map，切 CR3，设置 Hyper-V hook |
| `set_up_identity_map` | 建立 1GB page identity map |
| `load_identity_map_into_hyperv_cr3` | 把 identity map 临时写入 Hyper-V PML4 |
| `set_up_hyperv_hooks` | 在 Hyper-V text 区定位 VM-exit handler，并把 attachment detour 接入 |
| `find_hyperv_text_base/end` | 从 entry point 向前/向后找连续可执行页 |

`set_up_hyperv_hooks` 做了几件非常关键的事：

1. 切到 Hyper-V CR3。
2. 找 Hyper-V text 区。
3. 通过不同签名尝试识别 AMD 或 Intel 的 VM-exit handler 调用点。
4. AMD 路径还扫描 `get_vmcb_gadget`。
5. 调用 `hyperv_attachment_invoke_entry_point`，让 attachment 返回自己的 VM-exit detour 地址。
6. 在 Hyper-V text 区找连续 `0xCC` code cave。
7. 用 `hook_create` 把 code cave 变成跳转到 detour 的 trampoline。
8. 修改原 VM-exit handler call 的相对偏移，让它 call 到 code cave。

反作弊分析重点：

- Hyper-V text 段中连续 `0xCC` code cave 被改写。
- 某个 call rel32 被重定向到 code cave。
- code cave 内出现 `push imm32; mov dword ptr [rsp+4], imm32; ret` 形式的绝对跳转模板。
- Hyper-V CR3 早期被临时插入 identity map。

### 5. hyperv_attachment 加载

代码在 `src/bootloader/src/hyperv_attachment/hyperv_attachment.c`。

关键函数：

| 函数 | 作用 |
|---|---|
| `hyperv_attachment_set_up` | 总入口，加载文件、分配 heap、复制 PE、预留 PML4/PDPT 页 |
| `hyperv_attachment_load_and_delete_from_disk` | 从 EFI 分区读取 `hyperv-attachment.dll`，校验 MZ，然后删除文件 |
| `hyperv_attachment_set_up_heap` | 分配 runtime services data 页，包含 attachment image 和后续 heap |
| `hyperv_attachment_allocate_and_copy` | 按 PE header/section 把 attachment 复制到物理内存 |
| `hyperv_attachment_apply_relocation` | 根据实际映射基址应用 DIR64 relocation |
| `hyperv_attachment_get_relocated_entry_point` | 获取 relocation 后入口 |
| `hyperv_attachment_invoke_entry_point` | 调用 attachment 的 `entry_point`，让其返回 VM-exit detour |

关键全局变量：

| 变量 | 含义 |
|---|---|
| `hyperv_attachment_physical_base` | attachment PE 被复制后的物理基址 |
| `hyperv_attachment_heap_allocation_base` | 整个 attachment heap 的起始物理地址 |
| `hyperv_attachment_heap_allocation_usable_base` | 当前可分配游标 |
| `hyperv_attachment_heap_allocation_size` | 总 heap 大小 |
| `hyperv_attachment_heap_4kb_pages_reserved` | 预留页数，默认 2048 |
| `pml4_physical_allocation` / `pdpt_physical_allocation` | 给 hvloader 阶段 identity map 使用的页 |

可观测点：

- EFI 分区曾经存在 `\efi\microsoft\boot\hyperv-attachment.dll`，读取后删除。
- UEFI runtime services data 中出现一段连续页，包含 PE header/section 复制痕迹。
- 后续 hypervisor 会尝试隐藏这段 heap。

## UEFI hook 模板

`src/bootloader/src/hooks/hooks.c` 的 hook 模板固定为 14 字节：

```asm
push low32(target)
mov dword ptr [rsp+4], high32(target)
ret
```

这是一种 x64 绝对跳转写法。`hook_create` 保存原始字节，`hook_enable` 写 trampoline，`hook_disable` 恢复原始字节。

反作弊/取证识别点：

- 被 hook 函数开头出现 `68 ?? ?? ?? ?? C7 44 24 04 ?? ?? ?? ?? C3`。
- 原始代码开头 14 字节被替换。
- 该模板同时出现在 bootloader 阶段 hook 和 client 侧 kernel inline hook 构造中。

## Hypervisor 核心模块

### main.cpp

路径：`src/hypervisor/src/main.cpp`

这是 attachment 的运行时入口。

关键全局变量：

| 变量 | 含义 |
|---|---|
| `original_vmexit_handler` | 原始 Hyper-V VM-exit handler |
| `g_hyperv_attachment_physical_base` | attachment 物理基址 |
| `g_hyperv_attachment_page_count` | attachment 页数 |
| `heap_physical_initial_base` | heap 初始物理基址 |
| `heap_total_size_to_hide` | 准备隐藏的 heap 总大小 |
| `uefi_boot_physical_base_address` | UEFI bootloader 镜像物理基址 |
| `uefi_boot_image_size` | UEFI bootloader 镜像大小 |
| `g_ntoskrnl_base_address` | bootloader/winload 捕获或运行时发现的 ntoskrnl 基址 |

`entry_point` 初始化流程：

1. 保存原始 VM-exit handler。
2. 保存 UEFI boot image 地址和 heap 信息。
3. 保存 `ntoskrnl` 基址。
4. 把 `vmexit_handler_detour` 地址返回给 bootloader。
5. 映射 heap usable 区。
6. 初始化 `heap_manager`、`logs`、`slat`、`cr3_cache`。

`vmexit_handler_detour` 分发逻辑：

1. `process_first_vmexit()` 做首次初始化。
2. `cr3_cache::try_cache_on_vmexit()` 每次 VM-exit 都尝试采样 CR3。
3. 读取 VM-exit reason。
4. 如果是 CPUID，检查 RCX 中的 hypercall magic。
5. 如果是自定义 hypercall，调用 `hypercall::process`。
6. 如果是 SLAT violation，调用 `slat::process_slat_violation`。
7. 如果是 NMI，调用 `interrupts::process_nmi`。
8. 其他情况返回原始 Hyper-V handler。

首次 VM-exit 还会：

- 调用 `slat::process_first_vmexit()` 初始化 clean/hooked EPT 或 NPT。
- 调用 `interrupts::set_up()` 初始化 APIC/NMI 支持。
- 清零 UEFI bootloader 镜像内存。
- VM-exit 达到 10000 次后尝试隐藏 heap 页。

### arch

路径：`src/hypervisor/src/arch/`

这个模块抽象 Intel VMX 和 AMD SVM 差异。

`arch_config.h` 当前默认：

```cpp
#define _INTELMACHINE
```

说明默认走 Intel EPT / VMCS 路径。注释掉该宏后才走 AMD SVM / VMCB 路径。

主要能力：

- 读取 VM-exit reason。
- 判断 CPUID、EPT/NPT violation、CR3 write、NMI。
- 获取 guest CR3、RIP、RSP、GS base、CS selector、IDTR base。
- 设置 guest RIP/RSP。
- 根据 VM-exit instruction length 前进 guest RIP。
- Intel 路径使用 `__vmx_vmread` / `__vmx_vmwrite`。
- AMD 路径通过 `get_vmcb_gadget` 解析出读取 VMCB 的小函数。

### slat

路径：`src/hypervisor/src/slat/`

这是二级页表核心，Intel 下是 EPT，AMD 下是 NPT。

关键职责：

- 获取当前 SLAT root。
- 初始化 hook entry 池。
- 拆分 1GB/2MB 大页到 4KB 粒度。
- 添加/移除 SLAT code hook。
- 在执行访问时切到 shadow page。
- 隐藏物理页。
- 隐藏 hypervisor heap 范围。
- 刷新当前或所有 CPU 的 SLAT/TLB 缓存。
- Intel 下维护 clean EPT 和 hooked EPT。
- AMD 下维护 clean nested CR3 和 hooked nested CR3，并用 ASID 切换。

关键函数：

| 函数 | 作用 |
|---|---|
| `slat::set_up` | 建立 hook entry 池、dummy page、hooked NPT/EPT 基础结构 |
| `slat::process_first_vmexit` | Intel 深拷贝双 EPT；AMD 构造 identity mapped hooked NPT |
| `slat::add_slat_code_hook` | 把目标 GPA 的执行视图替换为 shadow page |
| `slat::remove_slat_code_hook` | 恢复目标 GPA 的原始映射 |
| `slat::hide_physical_page_from_guest` | Intel 清 EPT R/W/X；AMD 清 NPT present，使页对 guest 不可见 |
| `slat::try_hide_heap_pages` | 遍历 heap 物理页并隐藏 |
| `slat::process_slat_violation` | 根据访问类型在 clean/hooked 视图间切换 |
| `slat::map_guest_physical_to_host_physical` | 给 hidden memory 建立 GPA->HPA 的二级页表映射 |
| `slat::unmap_guest_physical` | 清除 GPA 映射 |

Intel 双 EPT逻辑：

- `clean_ept_cr3`: 保留原始读写执行视图。
- `hooked_ept_cr3`: 对 hook 页使用 execute-only shadow page。
- 执行访问切到 hooked EPT，读写访问切回 clean EPT。

AMD NPT逻辑：

- `hyperv_nested_cr3`: 原始 nested CR3。
- `hook_nested_cr3`: hook 视图 nested CR3。
- `clean_asid = 1`，`hooked_asid = 2`。
- violation 时切 nested CR3 和 ASID。

检测面：

- EPT/NPT 中同一 GPA 有 clean/hooked 两套视图。
- 大页被拆分成 4KB 页。
- hidden heap 对应 GPA 的 EPT R/W/X 全 0 或 NPT present 为 0。
- APIC/NMI 被用于跨核刷新。

### hypercall

路径：`src/hypervisor/src/hypercall/`

这是 VM-exit 上的服务端协议。client 侧执行 `CPUID`，hypervisor 在 CPUID VM-exit 中检查 RCX。

协议定义在 `src/common/hypercall/hypercall_def.h`：

```cpp
constexpr std::uint64_t hypercall_primary_key = 0x4E47;
constexpr std::uint64_t hypercall_secondary_key = 0x7F;
```

`hypercall_info_t` 位布局：

| 字段 | 位数 | 含义 |
|---|---:|---|
| `primary_key` | 16 | magic key，值为 `0x4E47` |
| `call_type` | 6 | `hypercall_type_t` |
| `secondary_key` | 7 | magic key，值为 `0x7F` |
| `call_reserved_data` | 35 | call-specific 数据 |

`virt_memory_op_hypercall_info_t` 会把 reserved 区拆成：

| 字段 | 位数 | 含义 |
|---|---:|---|
| `memory_operation` | 1 | 读/写 |
| `address_of_page_directory` | 34 | CR3 的 page directory PFN |

hypercall 类型表：

| ID | 类型 | 服务端作用 |
|---:|---|---|
| 0 | `guest_physical_memory_operation` | 读写 guest physical memory |
| 1 | `guest_virtual_memory_operation` | 按指定 CR3 读写 guest virtual memory |
| 2 | `translate_guest_virtual_address` | GVA -> GPA |
| 3 | `read_guest_cr3` | 返回当前 guest CR3 |
| 4 | `add_slat_code_hook` | 添加 SLAT code hook |
| 5 | `remove_slat_code_hook` | 移除 SLAT code hook |
| 6 | `hide_guest_physical_page` | 隐藏指定 GPA |
| 7 | `log_current_state` | 记录当前 trap frame |
| 8 | `flush_logs` | 把 hypervisor 日志复制给 client |
| 9 | `get_heap_free_page_count` | 返回 heap 空闲页数 |
| 10 | `get_process_base_by_pid` | 根据 PID 查进程 image base |
| 11 | `get_process_cr3` | 根据 PID 查进程 CR3 |
| 12 | `check_hyperv_attachment_memory_mapping` | 检查 attachment 内存映射状态，代码中枚举存在但主实现不完整 |
| 13 | `allocate_hidden_memory` | 给目标进程创建隐藏映射 |
| 14 | `free_hidden_memory` | 释放隐藏映射 |
| 15 | `get_process_eprocess_base` | 根据 PID 查 EPROCESS |
| 16 | `call_dllmain_silently` | 修改 guest context 以调用指定 DllMain |
| 17 | `dirbase_from_base_address` | 根据进程基址扫描/推断 dirbase |
| 18 | `hide_hypervisor_memory` | 尝试从 `MmPfnDatabase` 隐藏 hypervisor 物理页 |
| 19 | `restore_hypervisor_memory` | 恢复 PFN 数据库中的页信息 |
| 20 | `get_ntoskrnl_base_address` | 获取 boot 捕获或 KPCR 动态发现的 ntoskrnl 基址 |
| 21 | `update_ntoskrnl_base_address` | 更新 hypervisor 内部 ntoskrnl 基址 |
| 22 | `set_kernel_cr3` | 设置 kernel CR3 并重置内存管理初始化状态 |
| 23 | `get_ntoskrnl_base_from_kpcr` | 经 IDT/KPCR 相关逻辑发现 ntoskrnl |
| 24 | `get_system_process_cr3_from_kpcr` | 从 KPCR/KTHREAD/EPROCESS 路径发现当前进程对象 |
| 25 | `query_hypervisor_pfn_info` | 查询某物理页的 PFN 信息 |
| 26 | `get_hypervisor_memory_info` | 返回 attachment、heap、UEFI 镜像地址信息 |
| 27 | `test_export_discovery` | 测试导出解析和 `MmPfnDatabase` pattern |
| 28 | `enable_cr3_caching` | 兼容入口，当前注释为 deprecated |
| 29 | `disable_cr3_caching` | 兼容入口，当前注释为 deprecated |
| 30 | `get_cached_cr3` | 返回缓存的目标进程 CR3 |
| 31 | `get_cr3_cache_stats` | 返回 CR3 cache 统计 |
| 32 | `set_target_pid_for_cr3_caching` | 设置需要采样 CR3 的目标 PID |
| 33 | `get_target_pid_for_cr3_caching` | 当前实现返回 0 |

### hidden memory

服务端实现主要在 `src/hypervisor/src/hypercall/hypercall.cpp` 的 `allocate_hidden_memory` 和 `free_hidden_memory`。

分配流程：

1. 从 client 提供的参数结构读取目标 `virtual_address`、`size`、`target_pid`、`ps_initial_system_process`。
2. 根据目标 PID 找 CR3。
3. 检查目标 GVA 范围尚未映射。
4. 用 bump allocator 分配一段新的 GPA。
5. 从 hypervisor heap 分配真实 HPA page。
6. 通过 `slat::map_guest_physical_to_host_physical` 建立 GPA -> HPA。
7. 修改目标进程 guest 页表，把 GVA -> GPA。
8. flush 目标进程 TLB。
9. 把 GPA PFN 和 HPA PFN 放入 `g_hidden_memory_list`。

这不是普通 `VirtualAllocEx`。它在目标进程页表里插入映射，并通过 SLAT 把该 GPA 指到 hypervisor heap 中的真实页。

检测面：

- 目标进程页表出现不对应正常 VAD/内存管理路径的 PTE。
- GPA 范围由自定义 bump allocator 生成，起点为 `0x200000000`。
- `MmPfnDatabase`、VAD、working set 与实际可访问页之间可能不一致。
- hypervisor heap 中有与目标进程代码/PE section 对应的数据。

### memory_manager

路径：`src/hypervisor/src/memory_manager/memory_manager.*`

底层地址转换模块。

主要函数：

| 函数 | 作用 |
|---|---|
| `map_host_physical` | 把 host physical 映射到 `255ull << 39` 直接映射区 |
| `unmap_host_physical` | 从直接映射虚拟地址还原物理地址 |
| `map_guest_physical` | 通过 SLAT 把 GPA 翻译到 HPA，再映射为 host virtual |
| `translate_guest_virtual_address` | 按指定 guest CR3 手动页表遍历，支持 1GB/2MB/4KB 页 |
| `translate_host_virtual_address` | 手动翻译 host virtual |
| `operate_on_guest_virtual_memory` | 跨页读写指定 CR3 下的 guest virtual memory |

### memory_management

路径：`src/hypervisor/src/memory_manager/memory_management.*`

这是偏 Windows 内核内存管理的上层模块。

主要能力：

- 解析 `ntoskrnl.exe` 导出。
- 定位 `MmGetVirtualForPhysical`、`MmGetPhysicalMemoryRanges`。
- 通过 `MmGetVirtualForPhysical` 内部 pattern 定位 `MmPfnDatabase`。
- 查询 `_MMPFN`。
- 枚举/统计物理页样本。
- 尝试从 PFN 数据库中隐藏/恢复 hypervisor 页。
- 返回 hypervisor attachment/heap/UEFI 镜像的内存布局。

这部分对 Windows build 极其敏感，因为 `_MMPFN`、导出函数实现和 pattern 都不是稳定 ABI。

### ntoskrnl_parser

路径：`src/hypervisor/src/memory_manager/ntoskrnl_parser.*`

它在 guest 内存里解析 PE：

- 读 DOS header。
- 读 NT header。
- 找 export directory。
- 遍历 name/ordinal/function table。
- 按名称返回导出地址。
- 在指定内存范围里按字节模式搜索。

注意：`find_ntoskrnl_base_dynamically` 当前为了避免崩溃直接返回 0，主要依赖 boot 阶段捕获或 KPCR/IDT 方式发现。

### cr3_cache

路径：`src/hypervisor/src/cr3_cache/`

代码注释明确写着“bypass EAC's CR3 shuffling”。从行为上看，它是在每次 VM-exit 时机会性采样：

1. 统计 `total_samples`。
2. 检查 guest CPL 是否为 3。
3. 从 guest GS base + `0x40` 读取 TEB `ClientId.UniqueProcess`。
4. 如果 PID 匹配 `g_target_pid`，读取当前 guest CR3。
5. 如果 CR3 变化，更新 `g_cached_cr3` 和统计信息。

关键统计：

| 字段 | 含义 |
|---|---|
| `total_samples` | 总 VM-exit 采样次数 |
| `ring3_samples` | CPL=3 的样本数 |
| `target_pid_hits` | 命中目标 PID 次数 |
| `cr3_updates` | 缓存 CR3 更新次数 |
| `last_cached_cr3` | 最近缓存的 CR3 |

检测面：

- 大量 CPUID/hypercall 触发。
- VM-exit handler 中每次 exit 都读取 guest GS/TEB。
- 对特定 PID 的 CR3 长期缓存。
- 日志 marker `0xC300` 到 `0xC305`。

### interrupts / apic

路径：

- `src/hypervisor/src/interrupts/`
- `src/hypervisor/src/apic/`

作用：

- 初始化 xAPIC/x2APIC。
- 发送 NMI 到其他处理器。
- 在 NMI 中刷新 SLAT/TLB 状态。
- Intel 路径下设置 NMI handler gate。

这是为了让 SLAT hook 或隐藏页修改在多核上同步生效。

检测面：

- 非常规 NMI broadcast。
- IDT NMI gate 被修改。
- APIC ICR 写入 NMI。

### logs

路径：`src/hypervisor/src/logs/`

简单 ring-like 日志存储：

- 初始化时从 heap 分配 64 页日志区。
- `logs::add_log` 追加 `trap_frame_log_t`。
- `logs::flush` 把日志复制到 client buffer。

client 中 `fl` 命令会解析这些日志 marker，辅助判断 hypercall 和内存管理状态。

### crt / heap_manager

路径：

- `src/hypervisor/src/crt/`
- `src/hypervisor/src/memory_manager/heap_manager.*`

因为 attachment 运行环境不能依赖普通 CRT，所以项目提供极简运行时：

- `copy_memory`、`set_memory`、`compare`、`swap`、`min/max`
- 简单 mutex
- bitmap
- `chkstk.asm`
- 页级 free list heap

`heap_manager` 每次以 4KB 页为单位分配，是 SLAT hook、hidden memory、日志区的基础。

## Client 用户态控制端

### main 与 setup

入口：`src/client/src/main.cpp`

流程：

1. `sys::set_up()`。
2. 成功后进入交互循环。
3. 用户输入命令，由 `commands::process` 解析。
4. 输入 `exit` 退出。

`sys::set_up()` 在 `src/client/src/system/system.cpp`：

1. 检测 CPU vendor。
2. 调用 `hypercall::read_guest_cr3()` 验证 attachment 可响应。
3. 解析 `ntoskrnl.exe`。
4. 解析内核模块列表。
5. 设置 kernel hook helper。

如果 `read_guest_cr3` 返回 0，会打印 `hyperv-attachment doesn't seem to be loaded`。

### client hypercall 封装

路径：`src/client/src/hypercall/`

- `vmexit.asm`: `launch_raw_hypercall` 只执行 `cpuid; ret`。
- `hypercall.cpp`: 构造 `hypercall_info_t` 并封装成 C++ 函数。
- `hypercall.h`: 对外暴露 client 可用的 hypercall API。

client 到 hypervisor 的参数约定：

```text
RCX = hypercall_info_t
RDX = 参数 1
R8  = 参数 2
R9  = 参数 3
RAX = 返回值
```

### commands 命令表

命令定义在 `src/client/src/commands/commands.cpp`。

| 命令 | 作用 | 主要底层能力 |
|---|---|---|
| `rgpm` | 读取 guest physical memory | hypercall 0 |
| `wgpm` | 写入 guest physical memory | hypercall 0 |
| `cgpm` | GPA 到 GPA 拷贝 | hypercall 0 |
| `gvat` | GVA -> GPA | hypercall 2 |
| `rgvm` | 按 CR3 读取 GVA | hypercall 1 |
| `wgvm` | 按 CR3 写入 GVA | hypercall 1 |
| `cgvm` | GVA 到 GVA 拷贝 | hypercall 1 |
| `akh` | 添加 kernel code hook | SLAT hook + shadow page |
| `rkh` | 移除 kernel code hook | remove SLAT hook |
| `hgpp` | 隐藏 guest physical page | hypercall 6 |
| `fl` | 刷新/打印 hypervisor 日志 | hypercall 8 |
| `hfpc` | 查询 heap 空闲页数 | hypercall 9 |
| `lkm` | 列出内核模块 | client 本地解析 + hypercall 读内存 |
| `kme` | 列出内核模块导出 | PE parser |
| `dkm` | dump 内核模块到磁盘 | hypercall 读内存 |
| `gva` | 查询 alias 数值 | client alias map |
| `gpb` | 根据 PID 获取进程基址 | hypercall 10 |
| `chkmap` | 检查 attachment memory mapping | 部分实现 |
| `vuworld` | 特定 VALORANT/UWorld pattern 实验 | 特定目标研究代码 |
| `loaddll` | 常规路径 DLL 加载实验 | 实验分支 |
| `stealthdll` | hidden memory DLL 加载实验 | hidden memory + PE 手动映射 |
| `rs` | 读速测试 | hypercall 读内存 |
| `notepad_cr3` | 查 notepad CR3 | dirbase / process CR3 |
| `hide_hv_memory` | 从 PFN DB 隐藏 hypervisor memory | hypercall 18 |
| `test_ntoskrnl` | 测试 ntoskrnl 发现 | hypercall 20/23 |
| `test_exports` | 测试导出和 MmPfnDatabase 发现 | hypercall 27 |
| `pfn_query_demo` | PFN 查询演示 | hypercall 25/26 |
| `cr3_cache` | 设置目标 PID 并等待 CR3 cache | hypercall 30/31/32 |
| `cr3_monitor` | 连续监控 CR3 变化和读内存 | CR3 cache |
| `get_world` | 使用 cached CR3 读 VALORANT UWorld | 特定目标研究代码 |

### system 模块

路径：`src/client/src/system/`

职责：

- `NtQuerySystemInformation` 获取 kernel module list。
- `RtlAdjustPrivilege` 获取/恢复 SeDebugPrivilege。
- 用 hypercall dump 内核模块。
- 用 `portable_executable` 解析 PE export。
- 找 `PsActiveProcessHead`、`PsInitialSystemProcess`、`MmPhysicalMemoryBlock`。
- 找 `ntoskrnl` 中可执行 padding section，作为 kernel detour holder。
- 枚举进程 PID。
- 根据 PID 获取进程 base、PEB、模块基址、导出函数地址。

反作弊注意：

- 用户态会调用 Native API 查询系统模块。
- 会读取和 dump `ntoskrnl.exe`。
- 会在内存中解析导出表和扫描签名。
- 会查找可执行 `Pad` section 作为 detour holder。

### hook 模块

路径：`src/client/src/hook/`

作用：在 client 侧构造 kernel hook 需要的 shadow page 和 detour handler。

流程：

1. 找到 `kernel_detour_holder_base`。
2. `VirtualAlloc` + `VirtualLock` 分配 shadow page。
3. 通过 hypercall 把 shadow page VA 转成 GPA。
4. 把目标 kernel page 读入 shadow page。
5. 用 Zydis 对目标函数指令边界对齐。
6. 构造 inline hook bytes。
7. 构造 original bytes trampoline。
8. 调用 `hypercall::add_slat_code_hook`。

关键点：它不是直接改原始内核代码页，而是把执行访问切到 shadow page。因此传统代码完整性检查如果走读视图，可能看到原始字节。

检测面：

- 用户态 locked memory 被作为 shadow executable view。
- `kernel_detour_holder` 中出现 trampoline。
- EPT/NPT 中目标 GPA 的执行视图和读写视图不一致。
- Zydis 被用于 hook 指令边界分析。

### dll_loader 模块

路径：`src/client/src/dll_loader/`

这是实验性的 PE 手动映射模块。

能力：

- 读取 DLL 文件。
- 校验 MZ/PE。
- 计算 `SizeOfImage`。
- 写 PE header 和 section 到 hidden memory。
- 应用 relocation。
- 解析 import table。
- 尝试解析目标进程模块导出。
- 调用 `call_dllmain_silently`。

它和 `stealthdll` 命令绑定。核心不是正常远程线程注入，而是利用 hypervisor hidden memory 把 PE 映射到目标地址空间。

反作弊注意：

- 目标进程地址空间可出现没有正常 loader/VAD 记录的 PE 映射。
- IAT 解析可能留下不完整或异常导入状态。
- DllMain 调用路径依赖 hypervisor 修改 guest context。

### 特定目标实验代码

`commands.cpp` 中有大量硬编码：

- `VALORANT-Win64-Shipping.exe`
- `vgk.sys`
- `UWorld`
- `ShadowRegionsDataStructure`
- VGK 偏移，如 `0x838F8`、`0x83910`、`0x839C0`
- CR3 shuffling / EAC bypass 字样

这些不属于通用框架逻辑，而是特定反作弊/游戏环境研究代码。维护文档或做样本分析时建议把它们标为 target-specific experiment。

## src/common

路径：`src/common`

共享协议层：

| 文件 | 作用 |
|---|---|
| `hypercall/hypercall_def.h` | hypercall 类型、magic key、bitfield 编码 |
| `structures/memory_operation.h` | 读/写操作枚举 |
| `structures/trap_frame.h` | 通用寄存器 frame、日志 frame、NMI frame |

这是 client 和 hypervisor 必须同步的 ABI。任何 bitfield 宽度或 enum 顺序变化都会破坏协议兼容。

## tests/basic-test

路径：`tests/basic-test`

`hello.cpp` 是一个 DLL 样例：

- `DllMain(DLL_PROCESS_ATTACH)` 中 sleep。
- 写 `C:\temp\dll_log.txt`。
- 弹 `MessageBoxA`。

它主要用于验证 DLL loader / hidden memory / DllMain 调用实验路径，不是完整自动化测试。

## external

`external/` 与 `src/hypervisor/src/` 有大量重复文件，也包含额外实验模块，例如：

- `os_version.*`
- `tbi_discovery.*`
- 一份 hypervisor 源码快照

从当前 `hyper.sln` 看，主构建路径使用 `src/` 下工程；`external/` 更像迁移残留、备用实现或参考代码。分析时应优先看 `src/`，再用 `external/` 对照差异。

## 构建与环境

项目面向 Windows x64。

主要依赖：

- Visual Studio 2022
- MSVC v143
- Windows SDK 10.x
- MASM build customization
- EDK2 / VisualUefi 风格 UEFI 依赖
- vcpkg
- `cli11`
- `zydis`

注意事项：

- `hyper.sln` 主要配置是 `Release|x64`。
- `src/hypervisor/src/arch_config.h` 默认启用 Intel 路径。
- `src/bootloader/bootloader.vcxproj` 中存在本机绝对路径形式的 VisualUefi/EDK2 配置。
- `src/client/client.vcxproj` 中也有硬编码 include path。
- `src/client/vcpkg.json` 声明 `zydis` 和 `cli11`。
- 仓库包含 `src/client/vcpkg_installed/`，这是本地构建产物，不一定适合换机复用。

## 可观测检测面总结

### 磁盘/EFI 分区

- `\efi\microsoft\boot\bootmgfw.original.efi`
- `\efi\microsoft\boot\hyperv-attachment.dll`
- EFI 根目录 `\debug.txt`
- 被恢复/改写过的 `bootmgfw.efi` 时间戳和内容变化
- build 产物中 `hypervisor.dll` 可能被改名为 `hyperv-attachment.dll`

### UEFI/启动链内存

- bootmgfw/winload/hvloader 内部函数开头被 14 字节绝对跳转模板覆盖。
- Hyper-V text code cave 中出现 trampoline。
- `OslpBuildKernelMemoryMap` 路径中 loader descriptor 被改为 reserved。
- UEFI bootloader image 后续被 hypervisor 清零。
- attachment heap 位于 runtime services data 类型页。

### Hyper-V/VM-exit

- 原始 VM-exit handler call 被改到 code cave。
- CPUID VM-exit 中检查 `0x4E47` / `0x7F` magic。
- VM-exit handler 中额外执行 CR3 cache、SLAT violation 处理、NMI 处理。
- Intel 下存在 clean/hooked 双 EPT。
- AMD 下存在 clean/hooked NPT 和 ASID 切换。

### 内核地址空间

- 目标进程页表出现非正常内存管理路径建立的 GVA->GPA 映射。
- SLAT 中某些 GPA 指向 hypervisor heap HPA。
- 读写视图和执行视图不同。
- 大页被拆成 4KB 页。
- PFN 数据库可能被修改或与实际可访问内存不一致。
- `ntoskrnl` 被 dump 和 export/pattern 扫描。

### 用户态进程

- `client.exe` 使用 `CPUID` 作为 hypercall 入口。
- 使用 `NtQuerySystemInformation`、`RtlAdjustPrivilege`。
- 使用 `VirtualAlloc` + `VirtualLock` 分配 shadow page。
- 加载/链接 CLI11、Zydis。
- 控制台命令中出现 `cr3_cache`、`stealthdll`、`hide_hv_memory` 等明显语义。

### 日志 marker

部分重要 marker：

| Marker | 含义 |
|---:|---|
| `0xC0DE` | hypercall 入口记录 |
| `0xEEE1` - `0xEEE5` | ntoskrnl 基址获取 |
| `0xEEE6` - `0xEEE8` | ntoskrnl 基址更新 |
| `0xEEE9` - `0xEEEB` | kernel CR3 设置 |
| `0xEEEC` - `0xEEEE` | KPCR 方式找 ntoskrnl |
| `0xF100` - `0xF105` | KPCR traversal |
| `0xF000` - `0xF008` | PFN 查询 |
| `0xC300` - `0xC305` | CR3 cache |
| `0xE010` - `0xE018` | export 解析 |

## 关键代码阅读路线

建议按下面顺序读：

1. `src/common/hypercall/hypercall_def.h`
2. `src/bootloader/src/main.c`
3. `src/bootloader/src/bootmgfw/bootmgfw.c`
4. `src/bootloader/src/winload/winload.c`
5. `src/bootloader/src/hvloader/hvloader.c`
6. `src/bootloader/src/hyperv_attachment/hyperv_attachment.c`
7. `src/hypervisor/src/main.cpp`
8. `src/hypervisor/src/arch/arch.cpp`
9. `src/hypervisor/src/slat/slat.cpp`
10. `src/hypervisor/src/hypercall/hypercall.cpp`
11. `src/hypervisor/src/memory_manager/memory_manager.cpp`
12. `src/hypervisor/src/memory_manager/memory_management.cpp`
13. `src/hypervisor/src/cr3_cache/cr3_cache.cpp`
14. `src/client/src/hypercall/hypercall.cpp`
15. `src/client/src/system/system.cpp`
16. `src/client/src/hook/hook.cpp`
17. `src/client/src/dll_loader/dll_loader.cpp`
18. `src/client/src/commands/commands.cpp`

读完这条线，就能完整串起：

```text
启动链进入点
  -> PE 加载 hook
  -> Hyper-V VM-exit 接管
  -> CPUID hypercall
  -> SLAT/内存/CR3/hidden memory 能力
  -> 用户态命令如何调用
```

## 模块级结论

- `bootloader` 是落点和持久化/启动链附着层，重点看 EFI 文件、bootmgfw/winload/hvloader hook、TPM event log 相关代码。
- `hypervisor` 是能力层，重点看 VM-exit detour、hypercall 分发、SLAT 视图切换、hidden memory、PFN 数据库和 CR3 cache。
- `client` 是操控层，重点看 CPUID hypercall、命令表、kernel hook 构造、DLL 手动映射和特定目标实验代码。
- `common` 是协议层，重点看 magic key、bitfield 和 enum 顺序。
- `external` 不是主构建路径，更多像历史/参考代码。

从反作弊角度，这个样本最重要的不是单个用户态命令，而是三件事：

1. 它在 Windows 启动前接入 Hyper-V VM-exit 路径。
2. 它通过 SLAT/EPT/NPT 制造读写视图与执行视图差异。
3. 它把用户态控制命令伪装成 CPUID 触发的 hypercall。

