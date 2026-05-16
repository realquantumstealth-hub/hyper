# hyper

> **Official Forum / 官方论坛**: https://discord.gg/qslab

## Languages

[English](#en) · [中文](#zh) · [日本語](#ja) · [한국어](#ko) · [Русский](#ru) · [Українська](#uk) · [Tiếng Việt](#vi)

<a id="zh"></a>
## 中文说明

`hyper` 是一个多组件反作弊研究工程，按职责分层，主要包含：

- `src/bootloader/`：启动阶段组件（UEFI 相关）
- `src/hypervisor/`：核心底层逻辑
- `src/client/`：用户态客户端控制层
- `src/common/`：共享结构与协议定义
- `docs/`：架构与结构文档
- `tests/`：基础测试样例

### 反作弊视角

`hyper` 的价值在于提供“启动链 -> 低层执行层 -> 用户态控制层”的完整研究面，可用于评估更早期、更底层的对抗面与检测面：

- 启动阶段完整性：研究引导阶段对象替换、路径重定向、早期注入迹象
- 运行阶段控制：研究低层内存视图、执行流与中断处理的可观测性
- 用户态编排：研究控制指令、采样任务和反馈回路的安全性

### 可能作用与用途（防守用途）

- 构建分层防线：把检测点前移到 boot/early runtime
- 设计跨域关联检测：把 boot 事件与 runtime 行为关联
- 评估检测鲁棒性：验证单点检测在跨层对抗下的失效率

### 核心原理（高层）

1. 启动链介入：在系统早期阶段建立受控加载与校验路径
2. 低层监控：通过 hypervisor 层能力观察内存与执行状态
3. 协议通信：client 与低层通过统一协议交换采样与控制指令
4. 分析反馈：将低层观测结果回送到用户态用于策略决策

### 防守研究建议

- 把 boot 证据和 runtime 证据统一入库进行关联分析
- 对关键路径建立基线（中断、页表、关键模块映射）
- 将“是否异常”从单点判断升级为多信号评分模型

### 研究目标

- 研究 bootloader / hypervisor / client 的分层协作
- 研究接口协议、内存管理与中断处理模块组织
- 研究工程构建链与模块化调试方法

### 合规与边界

本项目仅用于安全研究与防御技术讨论，不用于任何未授权用途。

由于部分密钥、证书、可执行链路、绕过/注入成品等属敏感信息不方便在 GitHub 上公开，需要或想交流的同伴可以联系我们官方 Discord 进行深入探讨。

---

<a id="en"></a>
## English

`hyper` is a multi-component anti-cheat research project with layered architecture:

- `src/bootloader/`: boot-stage component (UEFI related)
- `src/hypervisor/`: core low-level logic
- `src/client/`: user-mode control layer
- `src/common/`: shared structures and protocol definitions
- `docs/`: architecture and structure documentation
- `tests/`: basic test samples

### Anti-Cheat Perspective

`hyper` is valuable because it exposes a full research surface from boot chain to low-level runtime to user-mode control:

- Boot-stage integrity: investigate replacement, path redirection, and early-stage tampering indicators
- Runtime control plane: analyze observability of memory views, execution flow, and interrupt handling
- User-mode orchestration: evaluate security of command, sampling, and feedback pipelines

### Potential Value and Use Cases (Defensive)

- Build layered defenses with earlier detection points (boot/early runtime)
- Correlate boot events with runtime behavior
- Evaluate robustness of single-point detection under cross-layer adversarial conditions

### Core Principles (High Level)

1. Boot-chain intervention to establish controlled load and validation paths
2. Low-level monitoring through hypervisor-layer observability
3. Unified protocol communication between client and low-level components
4. Feedback analysis in user-mode for strategy decisions

### Defensive Research Recommendations

- Store and correlate boot evidence with runtime evidence
- Baseline critical paths (interrupts, page tables, critical mappings)
- Move from binary alerts to multi-signal scoring

### Research Focus

- Layered cooperation across bootloader, hypervisor, and client
- Module organization for interface protocol, memory management, and interrupt handling
- Build pipeline and modular debugging workflow

### Compliance & Boundaries

This project is for security research and defensive technical discussion only, and must not be used for unauthorized purposes.

Some keys, certificates, executable chains, and bypass/injection deliverables are sensitive and are not suitable for public release on GitHub. For deeper discussion, please contact our official Discord.

---

<a id="ja"></a>
## 日本語

`hyper` は複数コンポーネントで構成されたアンチチート研究プロジェクトです。主な構成は以下です。

- `src/bootloader/`：起動段階コンポーネント（UEFI 関連）
- `src/hypervisor/`：中核となる低レベルロジック
- `src/client/`：ユーザーモード制御レイヤー
- `src/common/`：共通構造体とプロトコル定義
- `docs/`：アーキテクチャ文書
- `tests/`：基本テスト

### 研究目的

- bootloader / hypervisor / client のレイヤー連携の検証
- インターフェース、メモリ管理、割り込み処理の設計研究
- ビルドチェーンとモジュール化デバッグの検証

### コンプライアンス

本プロジェクトはセキュリティ研究および防御技術の議論のみを目的とします。

鍵・証明書・実行チェーン・バイパス/インジェクション成果物などの機微情報は GitHub で公開しません。詳細は公式 Discord へご連絡ください。

---

<a id="ko"></a>
## 한국어

`hyper`는 다중 컴포넌트 구조의 안티치트 연구 프로젝트입니다. 주요 구성은 다음과 같습니다.

- `src/bootloader/`: 부팅 단계 컴포넌트(UEFI 관련)
- `src/hypervisor/`: 핵심 저수준 로직
- `src/client/`: 사용자 모드 제어 계층
- `src/common/`: 공용 구조체 및 프로토콜 정의
- `docs/`: 아키텍처 문서
- `tests/`: 기본 테스트

### 연구 목표

- bootloader / hypervisor / client 계층 협업 구조 연구
- 인터페이스, 메모리 관리, 인터럽트 처리 모듈 연구
- 빌드 체인 및 모듈형 디버깅 워크플로우 연구

### 준수 및 범위

본 프로젝트는 보안 연구 및 방어 기술 논의 목적에만 사용됩니다.

키, 인증서, 실행 체인, 바이패스/인젝션 산출물 등 민감 정보는 GitHub에 공개하지 않습니다. 자세한 논의는 공식 Discord로 문의해 주세요.

---

<a id="ru"></a>
## Русский

`hyper` — много-компонентный исследовательский anti-cheat проект с послойной архитектурой:

- `src/bootloader/`: компонент стадии загрузки (UEFI)
- `src/hypervisor/`: ядро низкоуровневой логики
- `src/client/`: user-mode слой управления
- `src/common/`: общие структуры и определения протоколов
- `docs/`: архитектурная документация
- `tests/`: базовые тесты

### Цели исследования

- Изучение взаимодействия слоев bootloader / hypervisor / client
- Изучение структуры модулей интерфейса, памяти и прерываний
- Изучение сборочной цепочки и модульной отладки

### Соответствие и ограничения

Проект предназначен только для исследований безопасности и обсуждения defensive-подходов.

Ключи, сертификаты, исполняемые цепочки и готовые bypass/injection материалы не публикуются на GitHub. Для подробного обсуждения используйте наш официальный Discord.

---

<a id="uk"></a>
## Українська

`hyper` — багатокомпонентний дослідницький anti-cheat проєкт із шаровою архітектурою:

- `src/bootloader/`: компонент етапу завантаження (UEFI)
- `src/hypervisor/`: ядро низькорівневої логіки
- `src/client/`: user-mode шар керування
- `src/common/`: спільні структури й протоколи
- `docs/`: архітектурна документація
- `tests/`: базові тести

### Мета дослідження

- Вивчення взаємодії шарів bootloader / hypervisor / client
- Вивчення модулів інтерфейсу, керування пам’яттю та обробки переривань
- Вивчення пайплайна збірки та модульного налагодження

### Відповідність і межі

Проєкт призначено лише для безпекових досліджень і defensive-обговорень.

Ключі, сертифікати, виконувані ланцюги та готові bypass/injection матеріали не публікуються на GitHub. Для детального обговорення звертайтеся в наш офіційний Discord.

---

<a id="vi"></a>
## Tiếng Việt

`hyper` là dự án nghiên cứu anti-cheat đa thành phần với kiến trúc phân lớp:

- `src/bootloader/`: thành phần giai đoạn khởi động (liên quan UEFI)
- `src/hypervisor/`: logic mức thấp cốt lõi
- `src/client/`: lớp điều khiển user-mode
- `src/common/`: cấu trúc và định nghĩa giao thức dùng chung
- `docs/`: tài liệu kiến trúc
- `tests/`: bài kiểm thử cơ bản

### Mục tiêu nghiên cứu

- Nghiên cứu phối hợp giữa các lớp bootloader / hypervisor / client
- Nghiên cứu tổ chức mô-đun giao diện, bộ nhớ và xử lý ngắt
- Nghiên cứu chuỗi build và quy trình debug theo mô-đun

### Tuân thủ và phạm vi

Dự án chỉ dùng cho nghiên cứu bảo mật và thảo luận kỹ thuật phòng thủ.

Một số khóa, chứng chỉ, chuỗi thực thi và sản phẩm bypass/injection là thông tin nhạy cảm nên không công khai trên GitHub. Nếu cần trao đổi sâu hơn, vui lòng liên hệ Discord chính thức của chúng tôi.

