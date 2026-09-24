# wch-usb-driver

本项目是面向沁恒（WCH）微控制器的轻量级 USB 协议栈，提供统一的设备（Device）与主机（Host）驱动框架，将 USB 功能抽象为可配置、可复用的驱动库，避免在每个工程中重复搬运厂商示例代码。

驱动采用分层设计，各层职责如下：

| 层次 | 目录 | 职责 |
| --- | --- | --- |
| 芯片层 | `chips/` | 芯片 SDK、板级支持包（BSP）与构建配置，屏蔽具体型号差异 |
| 移植层 | `port/` | USB 控制器（IP）的寄存器操作与中断处理，直接对接硬件 |
| 核心层 | `src/`、`include/` | USB 协议实现：标准请求、控制传输、端点管理、事件分发 |
| 类驱动层 | `src/class/`、`include/class/` | 设备类协议实现（如 HID类、CDC类），向上提供类接口 |
| 应用层 | `examples/` | 基于驱动库的完整示例工程 |

## 支持的芯片

| 系列 | 芯片 | IP |
| --- | --- | --- |
| CH32V205 | ch32v203cct6 / ch32v205cct6 / ch32v205rct6 / ch32v205vct6 | usbfs / usbhs |

## 目录结构

```
wch-usb-driver/
├── chips/                      # 芯片层：SDK、板级支持包与构建配置
│   └── <family>/
│       ├── board/              # 板级支持包（BSP）：时钟、中断、USB 实例初始化
│       ├── sdk/                # 厂商 SDK：Core / Debug / Peripheral / Startup / Ld
│       ├── <chip>.mk           # 芯片级构建配置：USB 控制器基地址、支持的 IP
│       └── family.mk           # 系列级构建配置：工具链、头文件路径、编译与链接选项
├── docs/                       # 项目文档
├── examples/                   # 应用层：示例工程
│   ├── build.mk                # 示例通用构建规则（含输出目录、编译与链接规则）
│   ├── device/                 # 设备示例
│   ├── dual_device/            # 双设备示例
│   └── host/                   # 主机示例
├── include/                    # 对外头文件
│   ├── class/<class>/          # 类驱动头文件
│   ├── device/                 # 设备驱动头文件：public 供应用使用，private 供驱动内部与移植层使用
│   ├── host/                   # 主机驱动头文件：public 供应用使用，private 供驱动内部与移植层使用
│   ├── usb_config.h            # 驱动配置文件
│   ├── usb_define.h            # USB 协议定义文件
│   └── usb_driver.h            # 驱动总入口头文件：统一包含各头文件
├── port/                       # 移植层：USB 控制器寄存器操作
├── src/                        # 核心层：与硬件无关的协议实现
│   ├── class/<class>/          # 类驱动实现
│   ├── usbd_driver_core.c      # 设备驱动
│   └── usbh_driver_core.c      # 主机驱动
├── .clang-format               # 代码格式化规则
├── .vscode/settings.json       # VS Code 头文件路径与宏定义配置
└── README.md                   # 项目说明文件
```

## 快速开始

### 配置环境

1. 找到 MRS 安装目录（以默认安装路径为例，请按实际安装位置调整）：

   ```
   C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin
   C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC15\bin
   ```

2. 打开 `设置 → 系统 → 关于 → 高级系统设置 → 环境变量`，在 "系统变量" 或 "用户变量" 中找到 `Path`，点击“编辑 → 新建”，分别将上面路径添加进去，然后一路确定保存。

3. **打开终端** 使环境变量生效，验证配置:

   ```bash
   make -v
   riscv32-wch-elf-gcc -v
   ```

   能正常输出版本信息即配置成功。

### 编译

1. 进入示例路径：

   ```bash
   cd ./examples/device/hid_km
   ```

2. 编译指定芯片:

    ```bash
    make all -j8 CHIP=ch32v205rct6
    ```

3. 芯片同时支持多个 USB 控制器时，可指定 `IP` ：

    ```bash
    make all -j8 CHIP=ch32v205rct6 IP=usbfs
    ```

4. 下载程序

    - 输出文件位于 `examples/device/hid_km/build/<chip>/output/`，包含 `.elf`、`.bin`、`.hex`、`.lst` 及 `.map`。
    - 可通过 `WCH-LinkUtility` 或 `WCHISPStudio` 工具烧录到单片机。


5. 清理:

    ```bash
    make clear
    ```
