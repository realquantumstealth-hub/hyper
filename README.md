# hyper

[中文](#中文说明) | [English](#english)

## 中文说明

`hyper` 是一个多组件反作弊研究工程，项目按职责分层，主要包含：

- `src/bootloader/`：启动阶段组件（UEFI 相关）
- `src/hypervisor/`：核心低层逻辑
- `src/client/`：用户态客户端控制层
- `src/common/`：共享结构与协议定义
- `docs/`：架构与结构文档
- `tests/`：基础测试样例

### 研究目标

- 研究 bootloader / hypervisor / client 的分层协作
- 研究接口协议、内存管理与中断处理模块组织
- 研究工程构建链与模块化调试方法

### 合规与边界

本项目仅用于安全研究与防御技术讨论，不用于任何未授权用途。

**由于部分密钥、证书、可执行链路、绕过/注入成品等属敏感信息不方便在github上公开，需要或想交流的同伴可以联系我们官方discord进行深入探讨。**

---

## English

`hyper` is a multi-component anti-cheat research project with layered architecture:

- `src/bootloader/`: boot-stage component (UEFI related)
- `src/hypervisor/`: core low-level logic
- `src/client/`: user-mode control layer
- `src/common/`: shared structures and protocol definitions
- `docs/`: architecture and structure documentation
- `tests/`: basic test samples

### Research Focus

- Layered cooperation across bootloader, hypervisor, and client
- Module organization for interface protocol, memory management, and interrupt handling
- Build pipeline and modular debugging workflow

### Compliance & Boundaries

This project is for security research and defensive technical discussion only, and must not be used for unauthorized purposes.

**Some keys, certificates, executable chains, and bypass/injection deliverables are sensitive and are not suitable for public release on GitHub. If you need deeper discussion, please contact our official Discord.**


