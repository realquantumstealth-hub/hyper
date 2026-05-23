# hyper Project Analysis / hyper 项目分析说明

> Official Forum / 官方论坛: https://discord.gg/qslab

Languages: [中文](#说明) | [English](#english-version)

## 说明

本文档面向受控实验环境中的反作弊、内核安全、启动链安全分析人员。内容目标是把这个仓库的工程结构、启动链行为、Hyper-V 附着方式、hypercall 协议、用户态控制端能力、隐匿/规避相关实现和可观测检测面说明白。

`hyper` 不是普通应用，也不是常规 Windows 驱动。它是一套围绕 Windows UEFI 启动链和 Hyper-V VM-exit 路径构建的多阶段研究项目。它试图在 Windows 启动过程中把一个自定义运行时接入 Hyper-V 的 VM-exit handler，然后让用户态 `client.exe` 通过 `CPUID` 指令触发自定义 hypercall，从而获得跨地址空间读写、SLAT/EPT/NPT hook、隐藏页、CR3 缓存、PFN 查询等能力。

当前仓库明显继承了公开项目 `noahware/hyper-reV` 的核心思路：替换/链式启动 `bootmgfw.efi`，在 `bootmgfw`、`winload`、`hvloader` 阶段逐级 hook，最终把自定义代码接到 Hyper-V VM-exit handler。`hyper-reV` 原始说明中把 TPM/Measured Boot 视为会暴露 `uefi-boot` 的短板；本仓库额外加入了 `TpmMeasurementFilter.c`、`TpmLogSpoofer.c`、`PeHashCompute.c`，试图处理 TPM/TCG event log 中的 `bootmgfw.efi` 测量痕迹。

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

另一个重要特征是“Measured Boot 痕迹处理”。项目不是只做 Hyper-V 附着，还尝试在链式启动原始 `bootmgfw.efi` 前修改 UEFI 暴露的 TCG event log，让后续 Windows/远程证明看到的启动测量更接近合法链路。注意：这不是可靠意义上的“清空 TPM”。TPM PCR 是 extend-only 语义，正常情况下不能随意回滚；代码更准确的目标是修改内存中的 event log、删除或伪装异常 `bootmgfw.efi` 事件，并尝试用 TPM2 命令或 `HashLogExtendEvent` 做有限同步。

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

## 与公开雏形 hyper-reV 的关系

从结构和行为看，本仓库与 `noahware/hyper-reV` 的公开思路高度一致：

- 都以 UEFI bootloader 作为最早落点。
- 都围绕 `bootmgfw.efi`、`winload.efi`、`hvloader` 的加载过程放置 hook。
- 都把 payload 以 `hyperv-attachment.dll` 形式带入启动链。
- 都扫描 Hyper-V text 区，定位 VM-exit handler 相关调用点。
- 都通过改写 Hyper-V VM-exit 路径，让自定义 hypervisor runtime 先于原始 handler 执行。
- 都在 Windows 启动后由用户态控制端触发 VM-exit/hypercall。

关键差异在 TPM/Measured Boot：

- 原型说明中把 TPM 使用场景标成会暴露 `uefi-boot` 的问题点，也就是只要有 measured boot/attestation，替换过的启动组件会留下可信启动链证据。
- 当前仓库加入了 event log 删除、digest 伪装、PCR extend/Reset 尝试和原始 `bootmgfw.efi` hash 计算代码。
- 当前仓库的 `main.c` 里 TPM filter 初始化被注释，但 `bootmgfw_run_original_image` 在链式启动前仍调用 `CleanTpmBeforeChaining`。
- 因此它的 TPM 部分更像“在原型上补的对抗实验层”，还不能等价理解为完整绕过 measured boot。

反作弊分析时可以把 `hyper-reV` 当作主干模型，把当前仓库当作加入 TPM event log 对抗、CR3 cache、hidden memory、PFN DB 操作、目标游戏实验命令后的扩展版。

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
- `SetLegitimateBootmgfwInfo` 记录被链式加载的原始 `bootmgfw.efi` 内存基址和大小，后续 `TpmMeasurementFilter.c` 会用它区分“合法 bootmgfw 测量”和“替换阶段 bootloader/异常 bootmgfw 测量”。
- `CleanTpmBeforeChaining` 的调用时机很关键：它发生在原始 `bootmgfw.efi` 即将 `StartImage` 之前，目标是先修改当前 UEFI TCG event log，再把控制权交给真正的 Windows Boot Manager。

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

## TPM/TCG 与 Measured Boot

这一部分是当前仓库相对公开雏形 `hyper-reV` 最值得注意的增量。`hyper-reV` 的核心链路已经能完成 UEFI 阶段到 Hyper-V VM-exit 的接管，但它的原始说明里明确提到：在启用 TPM/Measured Boot/attestation 的机器上，启动测量会记录异常的 `uefi-boot` 或替换过的启动组件，远程证明/反作弊可以据此发现问题。

当前仓库加入了三类 TPM 相关代码：

| 文件 | 作用 | 状态判断 |
|---|---|---|
| `src/bootloader/src/TpmMeasurementFilter.c` | 获取 TCG/TCG2 protocol，读取 event log，删除或过滤异常 `bootmgfw.efi` 事件，保留被链式加载的原始 `bootmgfw.efi` 事件 | 主路径里部分接入，`CleanTpmBeforeChaining` 会被调用 |
| `src/bootloader/src/TpmLogSpoofer.c` | 直接扫描 TCG2 event log，尝试替换 `bootmgfw.efi` digest，并尝试 PCR extend/同步 | 更像实验性备用实现，当前主流程没有直接调用 `SpoofTpmEventLog` |
| `src/bootloader/src/PeHashCompute.c` | 计算原始 `bootmgfw.efi` 的 Authenticode 风格 SHA-256 hash，供 event log 伪装使用 | 代码存在，但主路径没有完整串起来 |
| `src/bootloader/src/TcgGuids.c` | 定义 TCG/TCG2 protocol GUID | 支撑 TPM 模块编译/定位协议 |

### 真实执行路径

`src/bootloader/src/main.c` 中原本有一段 TPM filter 初始化逻辑：

```c
//status = TpmMeasurementFilterEntry(image_handle, system_table);
```

这段目前被注释掉了，所以 `TpmMeasurementFilterEntry` / `InitializeTpmResetReplay` 不会在 UEFI 入口阶段主动执行。也就是说，`mOriginalTcg2Protocol`、`mOriginalTcgProtocol`、`mOriginalTcg2HashLogExtendEvent` 这些静态全局指针只有在别的路径初始化过时才可靠。

但 `src/bootloader/src/bootmgfw/bootmgfw.c` 的 `bootmgfw_run_original_image` 里确实会：

1. `LoadImage` 加载原始 `bootmgfw.efi`。
2. 通过 `get_image_info` 读取原始 `bootmgfw.efi` 的 `ImageBase` / `ImageSize`。
3. 调用 `SetLegitimateBootmgfwInfo` 记录合法 boot manager 的内存范围。
4. 放置 bootmgfw hook。
5. 在 `StartImage` 前调用 `CleanTpmBeforeChaining`。

所以当前仓库的意图很清楚：先让替换过的 UEFI bootloader 完成 Hyper-V attachment 准备，然后恢复并加载原始 `bootmgfw.efi`，最后在交给原始 boot manager 之前清理 event log 中能指向替换阶段的 `bootmgfw.efi` 测量。

### `TpmMeasurementFilter.c` 的逻辑

核心函数是 `CleanTpmBeforeChaining`：

```text
CleanTpmBeforeChaining
  -> RemoveBootmgfwFromEventLog
    -> GetCurrentEventLog
      -> TCG2 GetEventLog 或 TCG 1.2 StatusCheck
    -> 遍历 TCG 2.0 / TCG 1.2 event log
    -> IsBootmgfwMeasurementEvent
    -> 对命中的异常 bootmgfw event 做内存内压缩删除
    -> ZeroMem 清空尾部
```

`IsBootmgfwMeasurementEvent` 的判断方式：

- 只关注 `EV_EFI_BOOT_SERVICES_APPLICATION` 和 `EV_EFI_RUNTIME_SERVICES_DRIVER`。
- 事件数据至少要能解释为 `EFI_IMAGE_LOAD_EVENT`。
- 从 event data 里尝试解析路径，寻找 `bootmgfw.efi` 字符串。
- 如果事件的 `ImageLocationInMemory` / `ImageLengthInMemory` 等于 `SetLegitimateBootmgfwInfo` 记录的原始 `bootmgfw.efi`，就认为这是合法测量并保留。
- 其他包含 `bootmgfw.efi` 但内存范围不匹配的事件，被认为是替换阶段/异常 bootmgfw 测量并删除。

这不是清除 TPM 芯片里的 PCR。它是改 UEFI 暴露的 event log 内存区域。对反作弊来说，重要区别是：

- PCR 值仍然是硬件 TPM 内 extend 结果，正常情况下不能被任意回滚。
- event log 是用于解释 PCR 链的日志；如果日志被改而 PCR 没同步，就可能出现“event log replay 后的 PCR 和硬件 PCR 不一致”。
- 远程证明如果同时校验 PCR quote 和 event log replay，一致性可能失败。
- 如果某些本地/早期组件只读取 event log，不做强 quote 校验，内存中的 event log 篡改可能影响它们的观察结果。

### `TpmLogSpoofer.c` 的逻辑

`TpmLogSpoofer.c` 更像另一条实验路线，名字比实际更激进。它做了这些尝试：

- `LocateAndSpoofTpmEventLog`：通过 `EFI_TCG2_PROTOCOL->GetEventLog` 找 TCG2 event log，扫描事件，发现包含 `bootmgfw.efi` 的事件后替换 digest。
- `UpdatePCRDirectly`：调用 `HashLogExtendEvent` 并使用 `EFI_TCG2_EXTEND_ONLY`，意图只 extend PCR 不写 event log。
- `ExtendHashToTPM`：构造 `TPM2_PCR_Extend` 原始命令，通过 `SubmitCommand` 发送给 TPM。
- `ResetPCRToZero` / `ClearPCRValue`：尝试用零 hash extend 或 PCR_Reset 思路影响 PCR。
- `SynchronizeTPMWithEventLog`：注释承认 PCR extend 可能失败，也承认很多场景可能主要看 event log。

这些代码要作为“意图和实验痕迹”看，而不是作为稳定能力看。尤其是 `ClearPCRValue` 并不会真正把 boot PCR 清零，extend 零值只是继续改变 PCR；`TPM2_PCR_Reset` 对常见 boot PCR 也不会在正常启动上下文里随意成功。

### `PeHashCompute.c` 的逻辑

`PeHashCompute.c` 计算的是原始 `bootmgfw.efi` 的 Authenticode 风格 SHA-256 hash：

- 跳过 PE checksum。
- 跳过 security directory。
- 按 raw data 顺序 hash section。
- 排除签名区域。
- 将结果存入 `LegitimateBootmgfwHash[32]`。

这和 `TpmLogSpoofer.c` 的 digest 替换思路配套：先算出合法 bootmgfw 的 hash，再把 event log 里异常 bootmgfw 的 digest 改成合法 hash。不过当前主流程没有看到完整调用 `ComputeAndStoreLegitimateBootmgfwHash` -> `SpoofTpmEventLog` 的闭环。

### 反作弊检测重点

TPM/TCG 相关检测不要只问“TPM 有没有被清空”，更应该做一致性检查：

- 检查 TCG event log replay 得到的 PCR 是否等于 TPM quote 返回的 PCR。
- 检查 event log 中 `EV_EFI_BOOT_SERVICES_APPLICATION` 的 `bootmgfw.efi` 事件数量、顺序、路径、device path、image base/size 是否符合正常 Windows 启动。
- 检查 event log 尾部是否出现被压缩删除后的零填充、异常截断或 `LastEntry` 与实际事件边界不一致。
- 检查 `bootmgfw.efi` 文件时间戳/内容变化是否和 event log 中的测量摘要匹配。
- 检查是否存在先被测量的替换 bootloader 后又被恢复原始 `bootmgfw.efi` 的时间线矛盾。
- 检查 UEFI 内存中 TCG/TCG2 protocol 函数指针、event log buffer 所在内存类型、event log 区域是否在 `ExitBootServices` 前发生非固件路径写入。
- 对比 Windows 侧 `Get-TpmEndorsementKeyInfo`、系统 measured boot 日志、远程 attestation 服务记录和本地 EFI 分区文件状态。

从项目代码本身看，最直接的检测语义是：

```text
替换 bootmgfw/UEFI bootloader 已经被 TPM 测量
  -> 项目恢复原始 bootmgfw
  -> 项目在 StartImage 前修改 event log
  -> Windows 之后看到的日志可能缺失第一次异常测量
  -> 硬件 PCR 和篡改后日志 replay 可能不一致
```

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

### TPM/Measured Boot

- TCG/TCG2 event log 中 `bootmgfw.efi` 相关事件数量、顺序、digest 和 device path 异常。
- event log replay 得到的 PCR 与 TPM quote 的 PCR 不一致。
- event log buffer 尾部出现压缩删除后的零填充，或 `LastEntry` 与实际事件边界不一致。
- `EV_EFI_BOOT_SERVICES_APPLICATION` / `EV_EFI_RUNTIME_SERVICES_DRIVER` 中的 image base 和 image size 与正常 boot manager 加载行为不一致。
- EFI 分区中文件时间线显示替换、恢复、删除，但 measured boot 日志缺失对应事件。
- 启动早期存在对 TCG/TCG2 protocol、event log buffer 或 `HashLogExtendEvent`/`SubmitCommand` 的异常访问。

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
4. `src/bootloader/src/TpmMeasurementFilter.c`
5. `src/bootloader/src/TpmLogSpoofer.c`
6. `src/bootloader/src/PeHashCompute.c`
7. `src/bootloader/src/winload/winload.c`
8. `src/bootloader/src/hvloader/hvloader.c`
9. `src/bootloader/src/hyperv_attachment/hyperv_attachment.c`
10. `src/hypervisor/src/main.cpp`
11. `src/hypervisor/src/arch/arch.cpp`
12. `src/hypervisor/src/slat/slat.cpp`
13. `src/hypervisor/src/hypercall/hypercall.cpp`
14. `src/hypervisor/src/memory_manager/memory_manager.cpp`
15. `src/hypervisor/src/memory_manager/memory_management.cpp`
16. `src/hypervisor/src/cr3_cache/cr3_cache.cpp`
17. `src/client/src/hypercall/hypercall.cpp`
18. `src/client/src/system/system.cpp`
19. `src/client/src/hook/hook.cpp`
20. `src/client/src/dll_loader/dll_loader.cpp`
21. `src/client/src/commands/commands.cpp`

读完这条线，就能完整串起：

```text
启动链进入点
  -> TPM/TCG event log 处理
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

## English Version

This section is the English version of the analysis above. It is written for anti-cheat engineers, kernel security researchers, boot-chain analysts, and incident responders working in an authorized and controlled lab. It is not a deployment guide.

`hyper` is not a normal application and it is not a conventional Windows driver. It is a multi-stage Windows UEFI and Hyper-V research project. Its core idea is to enter the Windows boot chain early, attach a custom runtime to the Hyper-V VM-exit path, and let a user-mode `client.exe` trigger custom hypercalls through the `CPUID` instruction after Windows has booted.

At a high level, the project combines:

- pre-OS UEFI boot-chain attachment
- `bootmgfw.efi`, `winload.efi`, and `hvloader` hooks
- Hyper-V VM-exit handler detouring
- SLAT/EPT/NPT view switching
- CPUID-based hypercalls
- hidden memory experiments
- CR3 caching
- PFN database inspection and tampering experiments
- TPM/TCG measured boot event log manipulation attempts

The repository appears to inherit the main design from the public `noahware/hyper-reV` project. That original design already describes the UEFI boot module, restoration of `bootmgfw.efi`, `hyperv-attachment.dll`, Hyper-V VM-exit detouring, and the fact that TPM/measured boot/attestation can expose the earlier `uefi-boot` load. This repository adds TPM event log filtering/spoofing code, CR3 cache logic, hidden-memory functionality, PFN database logic, and target-specific experimental commands.

### Summary

The main execution chain is:

```text
UEFI bootloader
  -> restore and chain-load the original bootmgfw.efi
  -> hook bootmgfw PE loading
  -> observe winload.efi
  -> hook winload PE loading and memory map construction
  -> capture ntoskrnl.exe base and hvloader
  -> hook the Hyper-V launch path inside hvloader
  -> scan the Hyper-V text section for the VM-exit handler and a code cave
  -> attach the hyperv-attachment.dll VM-exit detour
  -> after Windows boots, client.exe enters the custom hypercall path through CPUID
```

From an anti-cheat perspective, the project is best understood as:

```text
pre-OS boot-chain attachment
  + Hyper-V VM-exit detour
  + second-level address translation view switching
  + user-mode command client
```

The TPM-related code should not be described as "clearing TPM" in a strict sense. TPM PCRs are extend-only in normal operation and cannot simply be rolled back. The more accurate description is that the project tries to modify the UEFI-exposed TCG event log in memory, remove or spoof suspicious `bootmgfw.efi` measurement events, and optionally attempt limited PCR synchronization through TPM2 commands or `HashLogExtendEvent`.

### Solution Layout

`hyper.sln` contains four Visual Studio projects:

| Project | Path | Main output | Language | Purpose |
|---|---|---|---|---|
| `bootloader` | `src/bootloader` | `.efi` | C | UEFI-stage boot-chain attachment component |
| `hypervisor` | `src/hypervisor` | `.dll` | C++20 / MASM | Runtime loaded into the Hyper-V context as a VM-exit detour |
| `client` | `src/client` | `.exe` | C++ / MASM | User-mode interactive controller |
| `basic-test` | `tests/basic-test` | `.dll` | C++ | Test DLL used by the manual/stealth loader experiments |

Repository layout:

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

Responsibilities:

- `bootloader`: decides when and how the attachment enters the boot chain.
- `hypervisor`: handles VM-exits, hypercalls, SLAT hooks, hidden memory, and memory-management operations.
- `client`: sends commands after Windows has booted.
- `common`: defines shared hypercall keys, structures, bitfields, and enums.

### Relationship To hyper-reV

The public `noahware/hyper-reV` project is the closest visible baseline. The shared design points are:

- a UEFI bootloader is used as the earliest execution point
- the original `bootmgfw.efi` is restored and chain-loaded
- `bootmgfw.efi`, `winload.efi`, and `hvloader` are observed or hooked in sequence
- `hyperv-attachment.dll` is loaded during the boot flow
- Hyper-V memory is scanned to find a VM-exit handler call site and a code cave
- a custom attachment runtime is inserted before the original Hyper-V VM-exit handler
- a user-mode client later communicates with the attachment through CPUID-triggered VM-exits

The important difference is TPM/measured boot handling:

- the original design warns that TPM and boot attestation can reveal the `uefi-boot` component through PCRs or measured boot logs
- this repository adds TCG event log deletion/spoofing attempts, PCR extend/reset experiments, and legitimate `bootmgfw.efi` hash calculation
- the TPM initialization call in `main.c` is currently commented out, but `bootmgfw_run_original_image` still calls `CleanTpmBeforeChaining`
- the TPM code should be treated as an experimental anti-analysis layer, not as a complete measured boot bypass

### Boot Chain Details

#### UEFI Entry

The UEFI entry point is `UefiMain` in `src/bootloader/src/main.c`.

Current high-level flow:

1. Print `=== Hyper UEFI Boot Manager ===`.
2. Restore the original `bootmgfw.efi`.
3. Set up the Hyper-V attachment.
4. Load and start the original `bootmgfw.efi` with hooks installed.

Important paths:

| Constant | Value | Meaning |
|---|---|---|
| `d_bootmgfw_path` | `\efi\microsoft\boot\bootmgfw.efi` | Windows Boot Manager path |
| `d_path_original_bootmgfw` | `\efi\microsoft\boot\bootmgfw.original.efi` | Backup path for the original boot manager |
| `d_hyperv_attachment_path` | `\efi\microsoft\boot\hyperv-attachment.dll` | Attachment payload loaded by the bootloader |

The TPM filter initialization in `main.c` is present but commented:

```c
//status = TpmMeasurementFilterEntry(image_handle, system_table);
```

That matters because some TPM module globals are only initialized if this entry point is called.

#### bootmgfw Stage

Code: `src/bootloader/src/bootmgfw/bootmgfw.c`

Important functions:

| Function | Purpose |
|---|---|
| `bootmgfw_restore_original_file` | Reads `bootmgfw.original.efi`, writes it back to `bootmgfw.efi`, restores metadata, and deletes the backup file |
| `bootmgfw_run_original_image` | Loads and starts the original boot manager through UEFI `LoadImage` / `StartImage` |
| `bootmgfw_place_hooks` | Installs a hook in the boot manager after retrieving its loaded image information |
| `bootmgfw_load_pe_image_detour` | Intercepts PE image loading and calls `winload_place_hooks` when `winload.efi` is observed |
| `parse_uefi_boot_image_info` | Records the physical base and size of the UEFI boot image for later cleanup |

Key observations:

- The project does not simply keep a malicious `bootmgfw.efi` in place. It restores the original boot manager before chain-loading it.
- The hook target is located through pattern scanning around the boot manager's PE loading path.
- `SetLegitimateBootmgfwInfo` records the original boot manager image base and size.
- `CleanTpmBeforeChaining` runs just before `StartImage`, attempting to clean the current TCG event log before execution is handed to the legitimate boot manager.

#### winload Stage

Code: `src/bootloader/src/winload/winload.c`

Important functions:

| Function | Purpose |
|---|---|
| `winload_place_hooks` | Installs the main winload hooks |
| `winload_place_load_pe_image_hook` | Hooks winload PE loading |
| `winload_load_pe_image_detour` | Observes images loaded by winload, especially `ntoskrnl.exe` and `hvloader` |
| `winload_place_oslp_build_kernel_memory_map_hook` | Hooks `OslpBuildKernelMemoryMap` |
| `winload_oslp_build_kernel_memory_map_detour` | Rewrites the loader memory map entry for the attachment heap |
| `WriteDebugFile` | Writes debug output to `\debug.txt` on the EFI partition |

Behavior:

- When `ntoskrnl.exe` is loaded, the project records `g_ntoskrnl_base_address`.
- When `hvloader` is loaded, the project calls `hvloader_place_hooks`.
- The memory map detour walks loader descriptors and changes the descriptor that matches `hyperv_attachment_heap_allocation_base` to memory type `24`, which is treated as reserved loader memory.
- `\debug.txt` on the EFI partition is a direct forensic artifact.

#### hvloader Stage

Code: `src/bootloader/src/hvloader/hvloader.c`

Important functions:

| Function | Purpose |
|---|---|
| `hvloader_place_hooks` | Finds and hooks the function that launches Hyper-V |
| `hvloader_launch_hv_detour` | Temporarily installs an identity map, switches into the Hyper-V CR3 context, and sets up Hyper-V hooks |
| `load_identity_map_into_hyperv_cr3` | Writes the temporary identity map into the Hyper-V PML4 |
| `set_up_hyperv_hooks` | Finds the Hyper-V text range, VM-exit handler call site, code cave, and installs the attachment detour |

The main hook operation is:

1. Switch into the Hyper-V CR3 context.
2. Locate the Hyper-V text range.
3. Find the VM-exit handler call site.
4. Invoke the attachment entry point.
5. Find a `0xCC` code cave.
6. Write a detour in the code cave.
7. Patch the original VM-exit handler call to reach the new detour.
8. Restore the temporary CR3 mapping state.

Detection points:

- modified call target inside the Hyper-V text section
- trampoline bytes written into a `0xCC` code cave
- temporary identity mapping inserted during Hyper-V launch
- architecture-specific Intel/AMD VM-exit signature scanning

#### hyperv_attachment Loading

Code: `src/bootloader/src/hyperv_attachment/hyperv_attachment.c`

Behavior:

- opens `\efi\microsoft\boot\hyperv-attachment.dll`
- validates the PE image
- deletes the file after loading it into memory
- allocates `EfiRuntimeServicesData` pages
- copies PE headers and sections
- applies relocations
- gets the relocated entry point
- calls the attachment entry point

Important globals:

| Global | Meaning |
|---|---|
| `hyperv_attachment_physical_base` | Physical base of the copied attachment image |
| `hyperv_attachment_heap_allocation_base` | Base of the full attachment heap allocation |
| `hyperv_attachment_heap_allocation_usable_base` | Current allocation cursor |
| `hyperv_attachment_heap_allocation_size` | Total heap size |
| `hyperv_attachment_heap_4kb_pages_reserved` | Reserved page count, default 2048 |
| `pml4_physical_allocation` / `pdpt_physical_allocation` | Page-table pages used for the temporary identity map |

Observable artifacts:

- the attachment file may exist briefly on the EFI partition
- runtime services memory contains copied PE headers and sections
- the heap is later hidden by the hypervisor side

### TPM/TCG And Measured Boot

This is the main addition over the public baseline design. The original `hyper-reV` documentation states that TPM and boot attestation can see the `uefi-boot` image through PCRs or measured boot logs. This repository tries to reduce that exposure by editing or spoofing the TCG event log.

TPM-related files:

| File | Purpose | Status |
|---|---|---|
| `src/bootloader/src/TpmMeasurementFilter.c` | Finds TCG/TCG2 protocols, reads the event log, removes suspicious `bootmgfw.efi` events, and preserves the legitimate boot manager event | Partially integrated, `CleanTpmBeforeChaining` is called |
| `src/bootloader/src/TpmLogSpoofer.c` | Scans TCG2 event logs, replaces `bootmgfw.efi` digests, and attempts PCR synchronization | Experimental fallback path, not called by the current main flow |
| `src/bootloader/src/PeHashCompute.c` | Computes an Authenticode-style SHA-256 hash for the legitimate `bootmgfw.efi` | Present but not fully wired into the main path |
| `src/bootloader/src/TcgGuids.c` | Defines TCG/TCG2 protocol GUIDs | Support code |

#### Actual Execution Path

The intended TPM flow is:

```text
bootmgfw_run_original_image
  -> LoadImage(original bootmgfw.efi)
  -> get_image_info
  -> SetLegitimateBootmgfwInfo
  -> bootmgfw_place_hooks
  -> CleanTpmBeforeChaining
  -> StartImage(original bootmgfw.efi)
```

`SetLegitimateBootmgfwInfo` records the memory base and size of the original boot manager. `CleanTpmBeforeChaining` then tries to remove `bootmgfw.efi` measurement events that do not match that legitimate image.

#### TpmMeasurementFilter.c

Core flow:

```text
CleanTpmBeforeChaining
  -> RemoveBootmgfwFromEventLog
    -> GetCurrentEventLog
      -> TCG2 GetEventLog or TCG 1.2 StatusCheck
    -> walk TCG 2.0 or TCG 1.2 events
    -> IsBootmgfwMeasurementEvent
    -> compact the log in memory by skipping suspicious events
    -> zero the remaining tail
```

`IsBootmgfwMeasurementEvent` checks:

- event type is `EV_EFI_BOOT_SERVICES_APPLICATION` or `EV_EFI_RUNTIME_SERVICES_DRIVER`
- event data looks like an `EFI_IMAGE_LOAD_EVENT`
- the event contains a `bootmgfw.efi` path
- the image base and size either match or do not match the legitimate boot manager recorded earlier

If the event matches the legitimate boot manager, it is preserved. If it references `bootmgfw.efi` but does not match the legitimate image, the code treats it as a suspicious earlier measurement and removes it from the in-memory event log.

Important distinction:

- this does not reset hardware PCRs
- it modifies the event log buffer exposed by firmware
- a strong attestation flow can replay the modified event log and compare it with a TPM quote
- if the replayed PCR does not match the hardware PCR, the event log manipulation becomes detectable

#### TpmLogSpoofer.c

This file implements a more experimental path:

- `LocateAndSpoofTpmEventLog` locates the TCG2 event log and replaces digests in `bootmgfw.efi` events
- `UpdatePCRDirectly` tries `HashLogExtendEvent` with `EFI_TCG2_EXTEND_ONLY`
- `ExtendHashToTPM` builds a raw `TPM2_PCR_Extend` command and sends it through `SubmitCommand`
- `ResetPCRToZero` and `ClearPCRValue` attempt zero-hash extension or PCR reset-like behavior
- `SynchronizeTPMWithEventLog` comments that PCR extension may fail and that some scenarios may only inspect the event log

These routines should be treated as experimental intent rather than a stable measured boot bypass. Extending zeros does not clear a PCR. `TPM2_PCR_Reset` is not generally available for normal boot PCRs in this context.

#### PeHashCompute.c

This file computes an Authenticode-style SHA-256 hash for the original `bootmgfw.efi`:

- skip PE checksum
- skip the security directory
- hash sections by raw file order
- exclude signature data
- write the result into `LegitimateBootmgfwHash[32]`

That supports the spoofing idea in `TpmLogSpoofer.c`: compute the legitimate boot manager hash, then use it to replace suspicious event log digests. In the current code, the full `ComputeAndStoreLegitimateBootmgfwHash` to `SpoofTpmEventLog` flow is not fully connected.

#### Anti-Cheat TPM Checks

Useful consistency checks:

- compare PCRs obtained from a TPM quote with PCRs replayed from the TCG event log
- inspect the number, order, path, digest, device path, image base, and image size of `bootmgfw.efi` events
- check for zero-filled tails or inconsistent `LastEntry` boundaries in the event log buffer
- compare `bootmgfw.efi` file timestamps and content with measured boot records
- look for a timeline where an altered boot manager was measured, then restored, but the log does not contain the corresponding event
- monitor early boot access to TCG/TCG2 protocol functions, event log buffers, `HashLogExtendEvent`, and `SubmitCommand`

The key detection idea is:

```text
the altered UEFI boot component was measured
  -> the project restores the original bootmgfw.efi
  -> the project edits the event log before StartImage
  -> Windows or attestation may see a log missing the earlier suspicious measurement
  -> hardware PCRs and replayed event-log PCRs may diverge
```

### UEFI Hook Template

`src/bootloader/src/hooks/hooks.c` uses a fixed 14-byte absolute jump template:

```asm
push low32(target)
mov dword ptr [rsp+4], high32(target)
ret
```

Forensics/detection:

- hooked functions may start with `68 ?? ?? ?? ?? C7 44 24 04 ?? ?? ?? ?? C3`
- the first 14 bytes of the original function are overwritten
- the same style appears in early boot hooks and in client-side kernel hook construction

### Hypervisor Core Modules

#### main.cpp

Code: `src/hypervisor/src/main.cpp`

The attachment entry point receives:

- original VM-exit handler address
- heap base and size
- UEFI boot image base and size
- attachment physical base and page count
- `ntoskrnl.exe` base

It initializes:

- heap manager
- logs
- SLAT subsystem
- CR3 cache

It returns `vmexit_handler_detour`, which becomes the custom handler placed before the original Hyper-V VM-exit handler.

`vmexit_handler_detour` performs:

1. first-exit processing
2. CR3 cache sampling
3. VM-exit reason dispatch
4. CPUID hypercall handling
5. SLAT violation handling
6. NMI handling
7. fallback to the original Hyper-V handler

#### arch

Code: `src/hypervisor/src/arch/`

This is the Intel/AMD abstraction layer.

- Intel path reads and writes VMCS fields.
- AMD path parses VMCB-related state through a gadget discovered during boot.
- It exposes guest RIP/RSP/CR3/GS/CS/IDTR, exit reason, RIP advancement, and TLB flush helpers.

`src/hypervisor/src/arch_config.h` defaults to Intel through `_INTELMACHINE`.

#### slat

Code: `src/hypervisor/src/slat/slat.cpp`

This is the second-level address translation layer.

Intel behavior:

- maintains clean and hooked EPT roots
- deep-copies Hyper-V SLAT structures
- switches views for hook behavior
- clears EPT permissions to hide selected physical pages

AMD behavior:

- uses clean and hooked NPT roots
- tracks clean and hooked ASIDs
- switches nested translation state for similar view separation

Main capabilities:

- `set_up`
- `process_first_vmexit`
- `add_slat_code_hook`
- `remove_slat_code_hook`
- `hide_physical_page_from_guest`
- `try_hide_heap_pages`
- `process_slat_violation`
- `map_guest_physical_to_host_physical`
- `unmap_guest_physical`
- large-page splitting

Detection surface:

- EPT/NPT roots that do not match normal Hyper-V layout
- clean/hooked view divergence
- large pages split into 4 KB pages around hooks
- GPA to HPA mappings that point into the attachment heap
- permission-cleared pages used to hide memory from the guest

#### hypercall

Code: `src/hypervisor/src/hypercall/hypercall.cpp`

The hypervisor watches CPUID VM-exits. If register values contain the expected magic keys from `src/common/hypercall/hypercall_def.h`, the exit is interpreted as a custom hypercall. Otherwise execution returns to Hyper-V.

Important keys:

| Field | Value |
|---|---|
| Primary key | `0x4E47` |
| Secondary key | `0x7F` |

Supported operation families include:

- read/write guest physical memory
- read/write guest virtual memory under a selected CR3
- translate guest virtual addresses
- read guest CR3
- add/remove SLAT code hooks
- hide guest physical pages
- flush logs
- query heap page count
- enumerate process base/CR3/EPROCESS data
- allocate/free hidden memory
- manually call DLL entry points
- hide/restore hypervisor memory in the PFN database
- query PFN information
- get/update `ntoskrnl.exe` base
- set kernel CR3
- KPCR-based kernel discovery
- export discovery
- CR3 cache control and statistics

This is the main command surface exposed to `client.exe`.

#### hidden memory

The hidden memory flow is implemented inside the hypercall layer. The idea is:

1. accept allocation parameters from the guest
2. find the target process CR3
3. reserve guest virtual address space
4. allocate fake guest physical addresses from a bump allocator
5. allocate real host physical pages from the attachment heap
6. map GPA to HPA through SLAT
7. write page-table entries in the target process
8. flush the relevant translation state
9. track the allocation in `g_hidden_memory_list`

Detection ideas:

- target process VAD/PTE/working-set state does not match normal Windows memory manager behavior
- guest virtual memory maps to GPA ranges that are not backed by normal guest physical memory
- GPA to HPA translation points into the hypervisor attachment heap
- PFN database and page-table state disagree

#### memory_manager

Code: `src/hypervisor/src/memory_manager/memory_manager.cpp`

This module handles low-level address translation:

- maps host physical memory into a direct-map region at `255ull << 39`
- translates guest physical memory through SLAT
- walks guest page tables for 1 GB, 2 MB, and 4 KB pages
- translates host virtual addresses
- reads and writes guest virtual memory across page boundaries

#### memory_management

Code: `src/hypervisor/src/memory_manager/memory_management.cpp`

This is Windows-kernel-specific memory logic:

- resolves `ntoskrnl.exe` exports
- locates `MmGetVirtualForPhysical`
- locates `MmGetPhysicalMemoryRanges`
- pattern-scans `MmGetVirtualForPhysical` to find `MmPfnDatabase`
- queries `_MMPFN`
- samples physical pages
- hides/restores hypervisor pages in the PFN database
- reports attachment, heap, and UEFI boot image memory ranges

This code is highly Windows-build-sensitive.

#### ntoskrnl_parser

Code: `src/hypervisor/src/memory_manager/ntoskrnl_parser.*`

This module parses PE structures in guest memory:

- DOS header
- NT header
- export directory
- name/ordinal/function tables
- named exports
- byte-pattern scans

`find_ntoskrnl_base_dynamically` currently returns 0 to avoid crashes, so the project primarily relies on boot-stage capture or KPCR/IDT-style discovery.

#### cr3_cache

Code: `src/hypervisor/src/cr3_cache/`

The comments explicitly mention bypassing EAC CR3 shuffling. Behavior:

1. sample on VM-exits
2. check whether guest CPL is 3
3. read guest GS base + `0x40` to identify `TEB.ClientId.UniqueProcess`
4. if the PID matches `g_target_pid`, read the current guest CR3
5. update `g_cached_cr3` and statistics when it changes

Detection surface:

- repeated guest GS/TEB reads from the VM-exit path
- long-lived CR3 cache for a selected PID
- CR3 cache log markers `0xC300` through `0xC305`

#### interrupts / apic

Code:

- `src/hypervisor/src/interrupts/`
- `src/hypervisor/src/apic/`

Purpose:

- initialize xAPIC/x2APIC logic
- send NMIs to other processors
- use NMI paths to synchronize SLAT/TLB state
- set an Intel NMI handler gate

Detection surface:

- unusual NMI broadcasts
- modified IDT NMI gate
- APIC ICR writes for NMI delivery

#### logs

Code: `src/hypervisor/src/logs/`

The logging system allocates 64 pages from the heap, appends `trap_frame_log_t` entries, and lets the client flush them with the `fl` command.

Markers include:

| Marker | Meaning |
|---:|---|
| `0xC0DE` | hypercall entry |
| `0xEEE1` - `0xEEE5` | `ntoskrnl` base retrieval |
| `0xEEE6` - `0xEEE8` | `ntoskrnl` base update |
| `0xEEE9` - `0xEEEB` | kernel CR3 setting |
| `0xF000` - `0xF008` | PFN query |
| `0xC300` - `0xC305` | CR3 cache |
| `0xE010` - `0xE018` | export resolution |

#### crt / heap_manager

Code:

- `src/hypervisor/src/crt/`
- `src/hypervisor/src/memory_manager/heap_manager.*`

The attachment cannot rely on a normal CRT, so the project provides:

- memory copy/set/compare helpers
- basic min/max/swap helpers
- simple mutex
- bitmap
- `chkstk.asm`
- page-based free-list heap

The heap is the backing allocator for SLAT hooks, hidden memory, logs, and internal runtime state.

### Client User-Mode Controller

#### main and setup

Entry point: `src/client/src/main.cpp`

Flow:

1. call `sys::set_up()`
2. enter the command loop
3. parse user commands through `commands::process`
4. exit on `exit`

`sys::set_up()` in `src/client/src/system/system.cpp`:

- detects CPU vendor
- calls `hypercall::read_guest_cr3()` to verify that the attachment responds
- parses `ntoskrnl.exe`
- parses kernel module information
- sets up the kernel hook helper

If the attachment is not active, it prints that `hyperv-attachment` does not seem to be loaded.

#### client hypercall wrapper

Code: `src/client/src/hypercall/`

- `vmexit.asm` implements `launch_raw_hypercall` as `cpuid; ret`
- `hypercall.cpp` builds hypercall structures and wraps them as C++ APIs
- `hypercall.h` exposes the client-side API

Register convention:

```text
RCX = hypercall_info_t
RDX = first argument
R8  = second argument
R9  = third argument
```

#### commands

Code: `src/client/src/commands/commands.cpp`

The command table includes:

| Command | Purpose |
|---|---|
| `rgpm` / `wgpm` / `cgpm` | read/write/copy guest physical memory |
| `gvat` | translate guest virtual address |
| `rgvm` / `wgvm` / `cgvm` | read/write/copy guest virtual memory |
| `akh` / `rkh` | add/remove kernel hooks |
| `hgpp` | hide a guest physical page |
| `fl` | flush hypervisor logs |
| `hfpc` | query heap free page count |
| `lkm` / `kme` / `dkm` | list/query/dump kernel modules |
| `gva` / `gpb` | get virtual aliases or process bases |
| `chkmap` | check mapping state |
| `loaddll` | manual DLL loading experiment |
| `stealthdll` | hidden-memory DLL loading experiment |
| `hide_hv_memory` | PFN database hiding experiment |
| `test_ntoskrnl` / `test_exports` | kernel base/export tests |
| `pfn_query_demo` | PFN query demo |
| `cr3_cache` / `cr3_monitor` | CR3 cache controls and monitoring |
| `vuworld` / `get_world` / `notepad_cr3` | target-specific experiments |

#### system

Code: `src/client/src/system/system.cpp`

This module provides user-mode discovery and helper routines:

- calls `NtQuerySystemInformation`
- calls `RtlAdjustPrivilege`
- parses kernel modules
- dumps kernel modules through hypercalls
- parses PE exports
- locates `PsActiveProcessHead`, `PsInitialSystemProcess`, and `MmPhysicalMemoryBlock`
- finds a kernel executable holder section for detours
- enumerates processes
- resolves module exports in target processes
- allocates locked shadow pages with `VirtualAlloc` and `VirtualLock`

#### hook

Code: `src/client/src/hook/hook.cpp`

This module builds SLAT-backed kernel hooks:

- allocates and locks a shadow page
- translates the shadow page to a GPA
- translates the target kernel address to physical memory
- reads the original page
- uses Zydis to compute instruction boundaries
- writes a 14-byte trampoline
- allocates detour code in the kernel detour holder
- adds the SLAT code hook

#### dll_loader

Code: `src/client/src/dll_loader/dll_loader.cpp`

This is a manual PE loader used with hidden memory experiments:

- reads DLL file bytes
- validates PE headers
- allocates hidden memory through hypercalls
- maps headers and sections
- applies relocations
- resolves imports
- calls `DllMain` through hypervisor-assisted logic

`tests/basic-test/hello.cpp` is a simple payload for this path. Its `DllMain` writes `C:\temp\dll_log.txt` and shows a message box.

#### Target-Specific Experiments

`commands.cpp` contains target-specific strings and offsets, including references to:

- `VALORANT-Win64-Shipping.exe`
- `vgk.sys`
- `UWorld`
- `ShadowRegionsDataStructure`
- EAC CR3 shuffling comments

These should be treated as research/debug experiments and also as strong semantic detection indicators.

### src/common

The shared protocol layer contains:

- hypercall magic keys
- hypercall enum values
- bitfield structures
- memory operation structures
- shared definitions used by both client and hypervisor

Important file:

```text
src/common/hypercall/hypercall_def.h
```

Important structure:

```c
typedef union _hypercall_info_t
{
    struct
    {
        UINT64 primary_key : 16;
        UINT64 call_type : 6;
        UINT64 secondary_key : 7;
        UINT64 call_reserved_data : 35;
    } bits;
    UINT64 flags;
} hypercall_info_t;
```

The client and hypervisor must agree on this layout exactly.

### tests/basic-test

This project builds a small DLL used by the loader experiments. It is not a full test suite. It provides a simple payload for validating whether manual or hidden-memory DLL loading reaches `DllMain`.

### external

The `external/` directory appears to contain reference or historical code, especially SLAT-related material. It is useful for comparison but it is not the main build path.

### Build And Environment Notes

The project targets Windows x64.

Main dependencies:

- Visual Studio 2022
- MSVC v143
- Windows SDK 10.x
- MASM build customization
- EDK2 / VisualUefi-style UEFI dependencies
- vcpkg
- `cli11`
- `zydis`

Notes:

- `hyper.sln` is primarily configured for `Release|x64`.
- `src/hypervisor/src/arch_config.h` defaults to the Intel path.
- `src/bootloader/bootloader.vcxproj` contains local VisualUefi/EDK2-style absolute paths.
- `src/client/client.vcxproj` also contains hardcoded include paths.
- `src/client/vcpkg.json` declares `zydis` and `cli11`.
- `src/client/vcpkg_installed/` is local build output and should not be assumed portable.

### Observable Detection Surface

#### Disk / EFI Partition

- `\efi\microsoft\boot\bootmgfw.original.efi`
- `\efi\microsoft\boot\hyperv-attachment.dll`
- EFI root `\debug.txt`
- restored or modified `bootmgfw.efi` timestamps/content
- `hypervisor.dll` renamed or deployed as `hyperv-attachment.dll`

#### UEFI / Boot Chain Memory

- 14-byte absolute jump templates at bootmgfw/winload/hvloader function entries
- trampoline code in Hyper-V text code caves
- loader memory map descriptor changed to reserved
- UEFI bootloader image zeroed later by the hypervisor
- attachment heap allocated as runtime services data

#### TPM / Measured Boot

- abnormal number/order/digest/device-path data for `bootmgfw.efi` events
- mismatch between event-log replay PCRs and TPM quote PCRs
- zero-filled event log tail after in-memory compaction
- inconsistent event log `LastEntry` boundaries
- image base/size fields that do not match normal Windows boot manager behavior
- EFI file timeline showing replacement/restoration while measured boot logs omit the corresponding event

#### Hyper-V / VM-exit

- original VM-exit handler call redirected to a code cave
- CPUID exits checking `0x4E47` / `0x7F`
- extra VM-exit logic for CR3 cache, SLAT violations, and NMI handling
- Intel clean/hooked EPT roots
- AMD clean/hooked NPT roots and ASID switching

#### Kernel Address Space

- target process mappings created outside normal Windows memory-management paths
- GPA to HPA mappings that point into the attachment heap
- read/write and execute views that differ
- large pages split into 4 KB pages
- PFN database state that does not match accessible memory
- `ntoskrnl` export/pattern scanning

#### User-Mode Process

- `client.exe` uses CPUID as its hypercall entry
- use of `NtQuerySystemInformation` and `RtlAdjustPrivilege`
- `VirtualAlloc` + `VirtualLock` for shadow pages
- linked CLI11 and Zydis usage
- command names such as `cr3_cache`, `stealthdll`, and `hide_hv_memory`

### Suggested Reading Path

Read these files in order:

1. `src/common/hypercall/hypercall_def.h`
2. `src/bootloader/src/main.c`
3. `src/bootloader/src/bootmgfw/bootmgfw.c`
4. `src/bootloader/src/TpmMeasurementFilter.c`
5. `src/bootloader/src/TpmLogSpoofer.c`
6. `src/bootloader/src/PeHashCompute.c`
7. `src/bootloader/src/winload/winload.c`
8. `src/bootloader/src/hvloader/hvloader.c`
9. `src/bootloader/src/hyperv_attachment/hyperv_attachment.c`
10. `src/hypervisor/src/main.cpp`
11. `src/hypervisor/src/arch/arch.cpp`
12. `src/hypervisor/src/slat/slat.cpp`
13. `src/hypervisor/src/hypercall/hypercall.cpp`
14. `src/hypervisor/src/memory_manager/memory_manager.cpp`
15. `src/hypervisor/src/memory_manager/memory_management.cpp`
16. `src/hypervisor/src/cr3_cache/cr3_cache.cpp`
17. `src/client/src/hypercall/hypercall.cpp`
18. `src/client/src/system/system.cpp`
19. `src/client/src/hook/hook.cpp`
20. `src/client/src/dll_loader/dll_loader.cpp`
21. `src/client/src/commands/commands.cpp`

That path connects:

```text
boot-chain entry
  -> TPM/TCG event log handling
  -> PE loading hooks
  -> Hyper-V VM-exit takeover
  -> CPUID hypercalls
  -> SLAT, memory, CR3, and hidden-memory features
  -> user-mode command surface
```

### Module-Level Conclusion

- `bootloader` is the entry and boot-chain attachment layer. Focus on EFI files, bootmgfw/winload/hvloader hooks, and TPM event log handling.
- `hypervisor` is the capability layer. Focus on the VM-exit detour, hypercall dispatch, SLAT view switching, hidden memory, PFN database logic, and CR3 cache.
- `client` is the control layer. Focus on CPUID hypercalls, the command table, kernel hook construction, manual DLL mapping, and target-specific experiments.
- `common` is the protocol layer. Focus on magic keys, bitfields, and enum ordering.
- `external` is reference/history material rather than the main build path.

For anti-cheat analysis, the three most important facts are:

1. The sample enters the Hyper-V VM-exit path before Windows is fully running.
2. It uses SLAT/EPT/NPT to create different read/write and execute views.
3. It exposes user-mode commands through CPUID-triggered custom hypercalls.
