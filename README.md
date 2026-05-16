# hyper

> **Official Forum / 官方论坛**: https://discord.gg/qslab

## Languages

[English](#en) · [中文](#zh)

<a id="en"></a>
## English

### Project Overview

`hyper` is a multi-project Windows / UEFI workspace organized around `hyper.sln`. The workspace is built from four central source areas:

- `src/bootloader/`
- `src/hypervisor/`
- `src/client/`
- `src/common/`

It also includes project-wide documentation, tests, a support source tree, and local build/output directories.

### What This Project Does

At a high level, `hyper` brings together several stages of a native systems-oriented workspace in one repository:

- a boot-stage project under `src/bootloader`
- a core low-level runtime project under `src/hypervisor`
- a usermode-side client under `src/client`
- a shared definition layer under `src/common`

This layout is useful for understanding how a multi-stage native workspace can keep:

- early-stage/bootstrap code
- core runtime logic
- usermode-facing control or utility code
- shared protocol and structure definitions

inside one solution instead of scattering them across unrelated repositories.

### High-Level Design

The workspace is organized around component boundaries rather than one single binary:

1. `src/bootloader` contains the earliest-stage project material and its support trees
2. `src/hypervisor` contains the central low-level runtime implementation
3. `src/client` contains the usermode-side project tree
4. `src/common` defines shared contracts used across the other components
5. `docs/`, `tests/`, and `external/` provide documentation, auxiliary validation targets, and support code

In architectural terms, the repository is structured as:

- stage-specific code in separate project roots
- shared definitions in one common subtree
- support material alongside the main source trees

### Root Directory Layout

The repository root contains:

- `hyper.sln`
- `README.md`
- `docs/`
- `tests/`
- `src/`
- `external/`
- `build/`

The entire project is centered on `src/` as the main source root.

### Workspace Structure

The workspace is split into four main project areas:

1. boot-stage code in `src/bootloader`
2. low-level runtime code in `src/hypervisor`
3. usermode client code in `src/client`
4. shared definitions in `src/common`

`docs/structure-overview.md` documents the same split and serves as the top-level project map.

### `src/bootloader`

`src/bootloader/` is a dedicated boot-stage project.

#### Top-level files and directories

- `bootloader.vcxproj`
- `bootloader.vcxproj.user`
- `uefi.default.props`
- `uefi.props`
- `build/`
- `ext/`
- `src/`

#### `src/bootloader/ext/`

This subtree includes:

- `edk2/`
- `ia32-doc/`
- `openssl/`

#### `src/bootloader/src/`

This subtree includes:

- `bootmgfw/`
- `disk/`
- `hooks/`
- `hvloader/`
- `hyperv_attachment/`
- `image/`
- `memory_manager/`
- `structures/`
- `winload/`
- `main.c`
- `PeHashCompute.c`
- `PeHashCompute.h`
- `TcgGuids.c`
- `TpmLogSpoofer.c`
- `TpmLogSpoofer.h`
- `TpmMeasurementFilter.c`
- `TpmMeasurementFilter.h`

`src/bootloader/` is organized as a full native project with:

- its own project file
- its own property sheets
- its own external dependency subtree
- its own internal source hierarchy

### `src/hypervisor`

`src/hypervisor/` is the core low-level project in the workspace.

#### Top-level files and directories

- `hypervisor.vcxproj`
- `hypervisor.vcxproj.filters`
- `hypervisor.vcxproj.user`
- `ext/`
- `src/`

#### `src/hypervisor/ext/`

This subtree includes:

- `ia32-doc/`

#### `src/hypervisor/src/`

This subtree includes:

- `apic/`
- `arch/`
- `core/`
- `cr3_cache/`
- `crt/`
- `hypercall/`
- `interrupts/`
- `logs/`
- `memory/`
- `memory_manager/`
- `slat/`
- `structures/`
- `arch_config.h`
- `main.cpp`

The hypervisor project separates:

- APIC-related code
- architecture code
- core definitions
- cache-related code
- runtime support
- wrapper/interface code
- interrupt handling
- logging
- memory handling
- SLAT-related code
- shared low-level structures

### `src/client`

`src/client/` is the usermode client project in the workspace.

#### Top-level files and directories

- `client.vcxproj`
- `client.vcxproj.filters`
- `client.vcxproj.user`
- `test_enum.cpp`
- `vcpkg.json`
- `ext/`
- `src/`
- `vcpkg_installed/`

#### `src/client/ext/`

This subtree includes:

- `portable_executable/`

#### `src/client/src/`

This subtree includes:

- `commands/`
- `dll_loader/`
- `hook/`
- `hypercall/`
- `system/`
- `main.cpp`
- `pfn_query_demo.cpp`

`src/client/` is a complete client-side native project with:

- its own project files
- its own source entry point
- its own command/loader/hook/system trees
- its own dependency manifest
- its own local dependency installation tree

### `src/common`

`src/common/` is the shared definition layer.

#### `src/common/hypercall/`

- `hypercall_def.h`

#### `src/common/structures/`

- `memory_operation.h`
- `trap_frame.h`

This area provides shared contracts used across the other project trees.

### `docs/`

`docs/` contains:

- `structure-overview.md`

This document is the most direct starting point for understanding:

- component boundaries
- directory roles
- naming conventions
- output layout

### `tests/`

`tests/` includes:

- `basic-test/`

The test project area contains its own Visual Studio project and sample source tree, which makes it a lightweight companion target inside the same workspace.

### `external/`

The root-level `external/` tree is a support source area that sits alongside the main `src/` workspace.

This area contains files related to:

- architecture helpers
- APIC support
- hypercall support
- interrupt handling
- memory management
- runtime support
- SLAT-related code
- logging and parser helpers

### Build and Output Footprint

The workspace keeps local build/output structure in multiple places:

- `build/`
- `src/bootloader/build/`
- `src/bootloader/build/x64/`
- `src/client/vcpkg_installed/x64-windows-static/`

This keeps the repository close to an actively used native development tree.

### Project Relationships

The workspace can be read as three executable/code-bearing tracks plus one shared layer:

1. `src/bootloader`
2. `src/hypervisor`
3. `src/client`
4. `src/common`

The shared documentation and support trees are:

- `docs/`
- `tests/`
- `external/`

### Recommended Reading Order

Recommended reading order:

1. `hyper.sln`
2. `docs/structure-overview.md`
3. `src/bootloader/bootloader.vcxproj`
4. `src/bootloader/src/`
5. `src/hypervisor/hypervisor.vcxproj`
6. `src/hypervisor/src/`
7. `src/client/client.vcxproj`
8. `src/client/src/`
9. `src/common/`
10. `tests/basic-test/`
11. `external/`

### Summary

`hyper` is a multi-component native workspace built from:

- a boot-stage project
- a core low-level project
- a usermode client project
- a shared common definition layer
- bundled support and documentation trees
- local build/output directories

<a id="zh"></a>
## 中文

### 项目概览

`hyper` 是一个围绕 `hyper.sln` 组织的 Windows / UEFI 多工程工作区。整个工作区主要由四个核心源码区域构成：

- `src/bootloader/`
- `src/hypervisor/`
- `src/client/`
- `src/common/`

同时还包含项目级文档、测试、支持源码树以及本地构建/输出目录。

### 项目作用

从工程层面看，`hyper` 把一套偏系统级的原生工作区不同阶段集中在同一个仓库里：

- `src/bootloader` 对应引导阶段工程
- `src/hypervisor` 对应核心低层运行时工程
- `src/client` 对应用户态侧工程
- `src/common` 对应跨组件共享定义层

这种布局适合用来理解一套多阶段原生工作区如何把以下内容放在同一套解决方案中：

- 早期阶段/引导阶段代码
- 核心运行时代码
- 用户态控制或工具代码
- 共享协议和结构定义

### 整体原理

整个工作区围绕“按组件边界拆分”而不是“单一二进制”来组织：

1. `src/bootloader` 放最早阶段项目及其支持树
2. `src/hypervisor` 放核心低层运行时实现
3. `src/client` 放用户态侧工程树
4. `src/common` 定义跨组件共享契约
5. `docs/`、`tests/`、`external/` 提供文档、辅助验证目标和支持代码

从架构角度看，整个仓库等于把下面几类内容分层放好：

- 按阶段拆分的工程根目录
- 单独的共享定义层
- 紧贴主源码树的支持材料

### 根目录结构

仓库根目录包含：

- `hyper.sln`
- `README.md`
- `docs/`
- `tests/`
- `src/`
- `external/`
- `build/`

整个项目以 `src/` 为主源码根目录。

### 工作区结构

工作区主要拆分为四个项目区域：

1. `src/bootloader` 中的引导阶段代码
2. `src/hypervisor` 中的低层运行时代码
3. `src/client` 中的用户态客户端代码
4. `src/common` 中的共享定义

`docs/structure-overview.md` 使用同样的划分方式描述整个项目，是最直接的整体地图。

### `src/bootloader`

`src/bootloader/` 是独立的引导阶段工程。

#### 顶层文件与目录

- `bootloader.vcxproj`
- `bootloader.vcxproj.user`
- `uefi.default.props`
- `uefi.props`
- `build/`
- `ext/`
- `src/`

#### `src/bootloader/ext/`

这一子树包含：

- `edk2/`
- `ia32-doc/`
- `openssl/`

#### `src/bootloader/src/`

这一子树包含：

- `bootmgfw/`
- `disk/`
- `hooks/`
- `hvloader/`
- `hyperv_attachment/`
- `image/`
- `memory_manager/`
- `structures/`
- `winload/`
- `main.c`
- `PeHashCompute.c`
- `PeHashCompute.h`
- `TcgGuids.c`
- `TpmLogSpoofer.c`
- `TpmLogSpoofer.h`
- `TpmMeasurementFilter.c`
- `TpmMeasurementFilter.h`

`src/bootloader/` 是一套完整的原生工程，包含：

- 自己的工程文件
- 自己的属性表
- 自己的外部依赖子树
- 自己的内部源码层级

### `src/hypervisor`

`src/hypervisor/` 是工作区中的核心低层工程。

#### 顶层文件与目录

- `hypervisor.vcxproj`
- `hypervisor.vcxproj.filters`
- `hypervisor.vcxproj.user`
- `ext/`
- `src/`

#### `src/hypervisor/ext/`

这一子树包含：

- `ia32-doc/`

#### `src/hypervisor/src/`

这一子树包含：

- `apic/`
- `arch/`
- `core/`
- `cr3_cache/`
- `crt/`
- `hypercall/`
- `interrupts/`
- `logs/`
- `memory/`
- `memory_manager/`
- `slat/`
- `structures/`
- `arch_config.h`
- `main.cpp`

该工程内部拆分为：

- APIC 相关代码
- 架构代码
- 核心定义
- 缓存相关代码
- 运行时支持
- 包装/接口代码
- 中断处理
- 日志
- 内存处理
- SLAT 相关代码
- 共享低层结构

### `src/client`

`src/client/` 是工作区中的用户态客户端工程。

#### 顶层文件与目录

- `client.vcxproj`
- `client.vcxproj.filters`
- `client.vcxproj.user`
- `test_enum.cpp`
- `vcpkg.json`
- `ext/`
- `src/`
- `vcpkg_installed/`

#### `src/client/ext/`

这一子树包含：

- `portable_executable/`

#### `src/client/src/`

这一子树包含：

- `commands/`
- `dll_loader/`
- `hook/`
- `hypercall/`
- `system/`
- `main.cpp`
- `pfn_query_demo.cpp`

`src/client/` 是一套完整的客户端侧原生工程，具备：

- 自己的工程文件
- 自己的入口
- 自己的 command / loader / hook / system 子树
- 自己的依赖清单
- 自己的本地依赖安装树

### `src/common`

`src/common/` 是共享定义层。

#### `src/common/hypercall/`

- `hypercall_def.h`

#### `src/common/structures/`

- `memory_operation.h`
- `trap_frame.h`

这一层为其他工程树提供共享契约。

### `docs/`

`docs/` 包含：

- `structure-overview.md`

这份文档是理解以下内容的最直接入口：

- 组件边界
- 目录职责
- 命名规则
- 输出布局

### `tests/`

`tests/` 包含：

- `basic-test/`

测试工程区域拥有自己的 Visual Studio 工程与样例源码树，是同一工作区中的轻量配套目标。

### `external/`

根目录的 `external/` 是与主 `src/` 工作区并行存在的支持源码区域。

这一部分包含与以下内容相关的文件：

- 架构辅助
- APIC 支持
- hypercall 支持
- 中断处理
- 内存管理
- 运行时支持
- SLAT 相关代码
- 日志与解析辅助

### 构建与输出痕迹

工作区在多个位置保留了本地构建/输出结构：

- `build/`
- `src/bootloader/build/`
- `src/bootloader/build/x64/`
- `src/client/vcpkg_installed/x64-windows-static/`

这些目录让整个仓库保持了真实原生开发工作树的形态。

### 工程关系

整个工作区可以理解为“三条主要工程线 + 一层共享定义”：

1. `src/bootloader`
2. `src/hypervisor`
3. `src/client`
4. `src/common`

配套的文档与支持树包括：

- `docs/`
- `tests/`
- `external/`

### 阅读建议

推荐顺序如下：

1. `hyper.sln`
2. `docs/structure-overview.md`
3. `src/bootloader/bootloader.vcxproj`
4. `src/bootloader/src/`
5. `src/hypervisor/hypervisor.vcxproj`
6. `src/hypervisor/src/`
7. `src/client/client.vcxproj`
8. `src/client/src/`
9. `src/common/`
10. `tests/basic-test/`
11. `external/`

### 总结

`hyper` 是一个多组件原生工作区，核心由以下部分构成：

- 引导阶段工程
- 核心低层工程
- 用户态客户端工程
- 共享公共定义层
- 内置文档与支持树
- 本地构建/输出目录
