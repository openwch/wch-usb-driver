配置文件
============================

本协议栈通过应用提供的 ``usb_config.h`` 进行功能裁剪和资源参数配置。``usb_driver.h`` 会自动包含该文件，因此 ``usb_config.h`` 所在目录必须位于头文件搜索路径中。可直接参考 ``examples/*/src/usb_config.h``。

驱动选择
--------------------

.. list-table::

   * - 宏
     - 说明
   * - ``USB_DEVICE_DRIVER_EN``
     - 使能设备驱动，编译 ``usbd_driver_core.c``
   * - ``USB_HOST_DRIVER_EN``
     - 使能主机驱动，编译 ``usbh_driver_core.c``

设备类和主机类驱动可同时使能。未定义对应宏时，相关代码不会被编译。

类驱动使能
--------------------

.. list-table::

   * - 宏
     - 说明
   * - ``USB_CLASS_HIDD_DRIVER_EN``
     - 使能 HID 设备类驱动
   * - ``USB_CLASS_CDCD_ACM_DRIVER_EN``
     - 使能 CDC-ACM 设备类驱动
   * - ``USB_CLASS_CDCD_ECM_DRIVER_EN``
     - 使能 CDC-ECM 设备类驱动
   * - ``USB_CLASS_HUBH_DRIVER_EN``
     - 使能 HUB 主机类驱动
   * - ``USB_CLASS_HIDH_DRIVER_EN``
     - 使能 HID 主机类驱动
   * - ``USB_CLASS_HIDH_KEYBOARD_EN``
     - 使能 HID 主机键盘解析，依赖 ``USB_CLASS_HIDH_DRIVER_EN``
   * - ``USB_CLASS_HIDH_MOUSE_EN``
     - 使能 HID 主机鼠标解析，依赖 ``USB_CLASS_HIDH_DRIVER_EN``
   * - ``USB_CLASS_CDCH_ACM_DRIVER_EN``
     - 使能 CDC-ACM 主机类驱动
   * - ``USB_CLASS_CDCH_ECM_DRIVER_EN``
     - 使能 CDC-ECM 主机类驱动

日志
--------------------

.. list-table::

   * - 宏
     - 说明
   * - ``USB_DRIVER_LOG_INFO_EN``
     - 输出信息级日志（``USB_LOGI``）
   * - ``USB_DRIVER_LOG_WARNING_EN``
     - 输出警告级日志（``USB_LOGW``）
   * - ``USB_DRIVER_LOG_ERROR_EN``
     - 输出错误级日志（``USB_LOGE``）
   * - ``USB_LOG_OUTPUT(format, ...)``
     - 日志输出接口，默认使用 ``printf``，可重定向到串口等

设备驱动参数
--------------------

.. list-table::

   * - 宏
     - 默认值
     - 说明
   * - ``USBD_REQUEST_CB_COUNT``
     - 12
     - 类请求回调的最大数量
   * - ``USBD_INTERFACE_CB_COUNT``
     - 2
     - 接口回调的最大数量

主机驱动参数
--------------------

.. list-table::

   * - 宏
     - 默认值
     - 说明
   * - ``USBH_DRIVER_MAX_COUNT``
     - 4
     - 可注册的主机类驱动数量
   * - ``USBH_DEVICE_POOL_SIZE``
     - 1
     - 可同时管理的设备数量
   * - ``USBH_ENDPOINT_POOL_SIZE``
     - 4
     - 主机端点池大小
   * - ``USBH_DESC_BUF_SIZE``
     - 512
     - 配置描述符缓冲区大小

HUB 主机类参数
--------------------

.. list-table::

   * - 宏
     - 默认值
     - 说明
   * - ``HUBH_HANDLE_POOL_SIZE``
     - 1
     - 可同时管理的 HUB 实例数量

HID 主机类参数
--------------------

.. list-table::

   * - 宏
     - 默认值
     - 说明
   * - ``USB_CLASS_HIDH_KEYBOARD_MAX_COUNT``
     - 1
     - 键盘实例数量上限
   * - ``USB_CLASS_HIDH_MOUSE_MAX_COUNT``
     - 1
     - 鼠标实例数量上限
   * - ``HIDH_HANDLE_POOL_SIZE``
     - 1
     - HID 主机句柄数量上限
   * - ``HIDH_REPORT_ITEM_POOL_SIZE``
     - 32
     - 报告描述符解析项池大小
   * - ``HIDH_MAX_INSTANCE_COUNT``
     - 5
     - 单个 HID 句柄可挂载的实例数量
   * - ``HIDH_REPORT_DESC_BUF_SIZE``
     - 512
     - 报告描述符缓冲区大小
   * - ``HIDH_GET_REPORT_BUF_SIZE``
     - 128
     - GET_REPORT 缓冲区大小
   * - ``HIDH_SET_REPORT_BUF_SIZE``
     - 128
     - SET_REPORT 缓冲区大小

CDC-ACM 主机类参数
--------------------

.. list-table::

   * - 宏
     - 默认值
     - 说明
   * - ``CDCH_ACM_HANDLE_POOL_SIZE``
     - 1
     - CDC-ACM 主机句柄数量上限

CDC-ECM 主机类参数
--------------------

.. list-table::

   * - 宏
     - 默认值
     - 说明
   * - ``CDCH_ECM_HANDLE_POOL_SIZE``
     - 1
     - CDC-ECM 主机句柄数量上限
