示例程序
============================

仓库在 ``examples/`` 下提供设备、主机和双设备三类示例。

.. list-table::

   * - 示例
     - 类型
     - 说明
   * - ``examples/device/hid_km``
     - 设备
     - HID 键鼠复合设备，按键模拟键盘和鼠标输入
   * - ``examples/device/cdc_acm``
     - 设备
     - CDC-ACM 虚拟串口，在 USB 主机与 USART 之间双向转发，LED 指示 DTR/RTS
   * - ``examples/device/cdc_ecm``
     - 设备
     - CDC-ECM 虚拟网卡，回复 ARP 请求和 ICMP echo
   * - ``examples/host/hid_km``
     - 主机
     - 枚举键鼠设备，打印按键、鼠标数据及 LED 状态
   * - ``examples/host/cdc_acm``
     - 主机
     - 枚举 CDC-ACM 设备，按键完成打开、关闭、发送及控制线操作
   * - ``examples/host/cdc_ecm``
     - 主机
     - 枚举 CDC-ECM 设备，通过 ARP 获取目标 MAC 并发送 ICMP echo 请求
   * - ``examples/dual_device/cdc_acm_bridge``
     - 双设备
     - 两个 CDC-ACM 实例间双向转发数据

- ``board_init()`` 以 921600 波特率初始化串口，用于输出启动信息和运行日志。
- ``CHIP`` 必须是示例支持的芯片，完整列表见 ``makefile`` 的 ``SUPPORTED_CHIPS`` 或 :doc:`supported_chips`。
- 产物位于 ``build/<chip>/output/``，包含 ``.elf``、``.bin``、``.hex``、``.lst`` 和 ``.map``。

可通过以下参数选择 USB 控制器：

.. list-table::

   * - 参数
     - 默认值
     - 说明
   * - ``USBD_INDEX``
     - 0
     - 设备示例使用的控制器
   * - ``USBH_INDEX``
     - 0
     - 主机示例使用的控制器
   * - ``USBD0_INDEX``
     - 0
     - 双设备示例的第一个控制器
   * - ``USBD1_INDEX``
     - 1
     - 双设备示例的第二个控制器


设备示例
--------------------

HID 键鼠复合设备
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

示例目录：``examples/device/hid_km``。

- 枚举为 HID 复合设备：接口 0 为键盘，接口 1 为鼠标；VID/PID 为 ``0x1A86/0xFE30``，全速运行。
- ``BUTTON_0``~``BUTTON_3`` 模拟 ``A``、``B``、左 ``Ctrl``、左 ``Shift``，支持组合键。
- ``BUTTON_4``~``BUTTON_7`` 控制鼠标相对移动，分别为 X+、X-、Y+、Y-，按住时持续发送位移量为 1 的报告。
- 主机通过 ``SET_REPORT`` 下发的键盘 LED 状态输出到 LED0~LED2，并打印到串口。
- 设备挂起时若有按键变化，先唤醒总线再发送报告。

CDC-ACM 虚拟串口
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

示例目录：``examples/device/cdc_acm``。

- 枚举为 CDC-ACM 设备，控制接口 0、数据接口 1；VID/PID 为 ``0x1A86/0xFE31``。
- 在 USB 主机与片上 USART 之间双向转发数据：主机发送的数据写入 USART，USART 接收的数据发回主机。
- DTR 状态输出到 LED0，RTS 状态输出到 LED1。
- 同时提供全速和高速配置描述符，实际速度取决于所选 USB 控制器。

CDC-ECM 虚拟网卡
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

示例目录：``examples/device/cdc_ecm``。

- 枚举为 CDC-ECM 设备，控制接口 0、数据接口 1；数据接口备用设置 0 无端点，备用设置 1 带两个批量端点；VID/PID 为 ``0x1A86/0xFE32``。
- 使用 ``board_get_mac_address()`` 返回的地址作为本机 MAC，并通过字符串描述符索引 3（序列号）和 4（``iMACAddress``）提供给主机；本机 IP 固定为 ``192.168.1.1``。
- 枚举完成后向主机发送网络连接状态和连接速度（100 Mbps）通知。
- 主机将数据接口切换到备用设置 1 时打开数据端点并启动读取，切回备用设置 0 时关闭数据端点。
- 收到发往本机 MAC 或广播地址的 ARP 请求时回复 ARP；收到发往本机 IP 的 ICMP echo 请求时回复 echo reply，其他报文丢弃。
- 同时提供全速和高速配置描述符：高速数据端点最大包长为 512，全速为 64。

主机示例
--------------------

HID 键鼠主机
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

示例目录：``examples/host/hid_km``。

- 使能主机、HUB 和 HID 类驱动，支持直连或经 HUB 连接的键盘和鼠标。
- 键盘状态变化时打印 LED 状态和当前按下的字母、数字键，并将 Lock 状态输出到 LED。
- HID 主机驱动在 Num Lock、Caps Lock、Scroll Lock 变化时，通过 ``SET_REPORT`` 同步键盘 LED。
- 鼠标变化时打印按键位图、累计坐标以及本次的 ``dx``、``dy``、``wheel``。

CDC-ACM 主机
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

示例目录：``examples/host/cdc_acm``。

- 枚举 CDC-ACM 设备并打印数量变化，使用第一个设备的句柄。
- 设备连接时点亮 LED0，端口打开时点亮 LED1。
- 持续读取数据，回调打印每次接收的字节数。
- 按键功能：

  .. list-table::

     * - 按键
       - 功能
     * - BUTTON_0
       - 打开端口，波特率在 9600、115200、460800、921600 间循环
     * - BUTTON_1
       - 关闭端口
     * - BUTTON_2
       - 发送 ``Hello, CDC-ACM!\n``
     * - BUTTON_3
       - 按住时置位 DTR
     * - BUTTON_4
       - 按住时置位 RTS
     * - BUTTON_5
       - 按住时发送 BREAK

CDC-ECM 主机
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

示例目录：``examples/host/cdc_ecm``。

- 使能主机、HUB 和 CDC-ECM 类驱动，支持直连或经 HUB 连接的 CDC-ECM 设备。
- 类驱动在设备绑定后自动完成 MAC 地址读取、数据接口关闭和以太网包过滤器配置，示例使用第一个设备的句柄。
- 本机 IP 固定为 ``192.168.1.2``，目标 IP 为 ``192.168.1.1``；端口打开且连接建立后发送 ARP 请求获取目标 MAC。
- 持续读取数据，收到 ARP 应答时打印目标 MAC，收到 ICMP echo reply 时打印序号。
- LED0 指示 CDC-ECM 设备存在，LED1 指示端口已打开。
- 按键功能：

  .. list-table::

     * - 按键
       - 功能
     * - BUTTON_0
       - 打开 / 关闭端口；打开时注册打开、连接状态与速度变化回调
     * - BUTTON_1
       - ARP 解析完成后发送一次 ICMP echo 请求（ping）

双设备示例
--------------------

CDC-ACM 双向桥
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

示例目录：``examples/dual_device/cdc_acm_bridge``。

- 两个 USB 实例均枚举为 CDC-ACM 设备，VID/PID 为 ``0x1A86/0xFE31``。
- 实例 0 收到的数据转发给实例 1，实例 1 收到的数据转发给实例 0，写完成后继续读取源实例。
- LED0 指示实例 0 到实例 1 的转发，LED1 指示反方向；对应写入完成后熄灭。
- 仅在对端完成枚举后转发，否则丢弃本次数据并重新启动读操作。
- 要求芯片具备两个 USB 控制器，``USBD0_INDEX`` 与 ``USBD1_INDEX`` 不能相同。
