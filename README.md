# wch-usb-driver

本项目是面向沁恒（WCH）微控制器的轻量级 USB 协议栈，提供统一的设备（Device）与主机（Host）驱动框架。

## 支持的芯片

| 系列 | 芯片 | IP（索引） |
| --- | --- | --- |
| ch32v4x7 | ch32v407ret6 / ch32v407vet6 / ch32v407weu6 / ch32v467ret6 / ch32v467vet6 / ch32v467weu6 | usbhs1 (0) / usbhs2 (1) |
| ch32v30x | ch32v303cbt6 / ch32v303rbt6 / ch32v303rct6 / ch32v303rct7 / ch32v303vct6 / ch32v305cct6 / ch32v305fbp6 / ch32v305gbu6 / ch32v305rbt6 / ch32v307rct6 / ch32v307vct6 / ch32v307wcu6 / ch32v317vct6 / ch32v317wcu6 | usbfs (0) |
| ch32v205 | ch32v203cct6 / ch32v205cct6 / ch32v205rct6 / ch32v205vct6 | usbfs (0) / usbhs (1) |

## 目录结构

```
wch-usb-driver/
├── chips/                      # 芯片层：SDK、板级支持包与构建配置
│   └── <family>/
│       ├── board/              # 板级支持包（BSP）：时钟、中断、USB 实例初始化
│       ├── sdk/                # 厂商 SDK：Core / Debug / Peripheral / Startup / Ld
│       ├── <chip>.mk           # 芯片级构建配置
│       └── family.mk           # 系列级构建配置
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

3. 下载程序

    - 输出文件位于 `examples/device/hid_km/build/<chip>/output/`，包含 `.elf`、`.bin`、`.hex`、`.lst` 及 `.map`。
    - 可通过 `WCH-LinkUtility` 或 `WCHISPStudio` 工具烧录到单片机。

4. 清理:

    ```bash
    make clear
    ```
