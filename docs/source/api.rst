API 参考
============================

本章按设备（Device）与主机（Host）两类驱动，说明应用层可调用的公共 API。以下接口均声明于 ``include/`` 目录下的公共头文件中，``*_private.h`` 中的内部接口不对外开放。应用包含 ``usb_driver.h`` 即可获得全部公共声明。

驱动及相关功能通过 ``usb_config.h`` 中的 ``USB_DEVICE_DRIVER_EN``、``USB_HOST_DRIVER_EN`` 和 ``USB_CLASS_*_DRIVER_EN`` 等宏进行裁剪，未启用的驱动不参与编译，其接口也不可用。

设备
----------------------------

核心驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

设备核心驱动负责 USB 设备的枚举、标准请求处理与端点数据传输，并通过事件回调向应用层通知复位、挂起、SOF 和枚举完成等状态。应用通过板级函数 ``board_usb_init(index, USB_MODE_DEVICE)`` 获取设备句柄。

.. note::

   设备事件回调及端点完成回调均在 USB 中断上下文中执行，回调内应避免耗时操作，也不要调用可能阻塞的接口。

.. c:type:: usbd_handle_t

   设备驱动句柄，类型不透明。由 ``board_usb_init()`` 返回，应用无需了解其内部结构。

.. c:enum:: usbd_event_t

   设备事件类型，取值如下。

   .. list-table::

      * - 取值
        - 数值
        - 说明
      * - ``USBD_EVENT_RESET``
        - 0
        - 设备收到 USB 总线复位。
      * - ``USBD_EVENT_SUSPEND``
        - 1
        - 设备进入挂起状态。
      * - ``USBD_EVENT_SOF``
        - 2
        - 收到帧起始（SOF）事件。仅当 ``usbd_drv_open()`` 的 ``sof_en`` 为 ``true`` 时产生。
      * - ``USBD_EVENT_ENUM_COMPLETED``
        - 3
        - 枚举完成，即主机发送 ``SET_CONFIGURATION`` 请求之后。
      * - ``USBD_EVENT_COUNT``
        - 4
        - 事件数量标记，不对应实际事件。

.. c:struct:: usbd_event_ctx_t

   设备事件上下文。回调执行期间有效，回调返回后应用不应继续保存其指针。

   .. list-table::

      * - 成员
        - 类型
        - 适用事件
        - 说明
      * - ``e``
        - ``usbd_event_t``
        - 全部
        - 事件类型。
      * - ``sof.frame_num``
        - ``uint16_t``
        - ``USBD_EVENT_SOF``
        - 帧号。
      * - ``sof.mframe_num``
        - ``uint16_t``
        - ``USBD_EVENT_SOF``
        - 微帧号。
      * - ``enum_completed.config_num``
        - ``uint8_t``
        - ``USBD_EVENT_ENUM_COMPLETED``
        - 主机选择的配置值。
      * - ``enum_completed.link_speed``
        - ``uint8_t``
        - ``USBD_EVENT_ENUM_COMPLETED``
        - 实际链路速度，取值见 ``usb_speed_t``。

.. c:type:: usbd_event_cb

   设备事件回调函数类型。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbd_handle_t *``
        - 产生事件的设备句柄。
      * - ``ctx``
        - ``usbd_event_ctx_t *``
        - 事件上下文。

.. c:type:: usbd_get_desc_cb

   标准描述符获取回调函数类型。核心驱动收到标准 ``GET_DESCRIPTOR`` 请求时调用该回调。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbd_handle_t *``
        - 设备句柄。
      * - ``type``
        - ``uint8_t``
        - 描述符类型，取 ``usb_desc_type_t`` 中的值。
      * - ``index``
        - ``uint8_t``
        - 同类描述符的索引。
      * - ``speed``
        - ``usb_speed_t``
        - 当前 USB 速度。
      * - ``len``
        - ``size_t *``
        - 输出参数，回调需写入描述符长度。

   .. list-table::

      * - 返回值
        - 说明
      * - 描述符首地址
        - 回调成功时返回描述符的首地址。
      * - ``NULL``
        - 不支持该描述符。

.. c:function:: bool usbd_drv_open(usbd_handle_t *h, usb_speed_t speed, bool sof_en, usbd_get_desc_cb get_desc_cb)

   打开 USB 设备驱动，初始化 USB 控制器并响应主机枚举。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbd_handle_t *``
        - 设备驱动句柄，由 ``board_usb_init()`` 获取。
      * - ``speed``
        - ``usb_speed_t``
        - 期望的 USB 速度，支持 ``USB_SPEED_LOW``、``USB_SPEED_FULL`` 和 ``USB_SPEED_HIGH``，具体支持范围取决于所用的 USB 控制器。
      * - ``sof_en``
        - ``bool``
        - 是否使能 SOF 事件。为 ``true`` 时会触发 ``USBD_EVENT_SOF`` 回调。
      * - ``get_desc_cb``
        - ``usbd_get_desc_cb``
        - 标准描述符获取回调，不可为 ``NULL``。驱动会在打开时通过该回调读取设备描述符以确定端点 0 的最大包长。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 打开成功。
      * - ``false``
        - 参数非法、速度不支持或描述符无效。

.. c:function:: bool usbd_drv_close(usbd_handle_t *h)

   关闭 USB 设备驱动，停止 USB 控制器工作。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbd_handle_t *``
        - 设备驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 关闭成功。
      * - ``false``
        - 关闭失败。

.. c:function:: bool usbd_drv_resume(usbd_handle_t *h)

   在设备挂起状态下发送恢复（Resume）信号，用于远程唤醒主机。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbd_handle_t *``
        - 设备驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 恢复信号发送成功。
      * - ``false``
        - 主机未通过 ``SET_FEATURE(DEVICE_REMOTE_WAKEUP)`` 使能远程唤醒。

.. c:function:: bool usbd_register_event_callback(usbd_handle_t *h, usbd_event_t event, usbd_event_cb cb)

   注册设备事件回调。同一事件只能注册一个回调，重复注册会替换原有回调。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbd_handle_t *``
        - 设备驱动句柄。
      * - ``event``
        - ``usbd_event_t``
        - 要注册的事件，不能为 ``USBD_EVENT_COUNT``。
      * - ``cb``
        - ``usbd_event_cb``
        - 事件回调函数。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注册成功。
      * - ``false``
        - 参数非法。

.. c:function:: bool usbd_unregister_event_callback(usbd_handle_t *h, usbd_event_t event)

   注销设备事件回调。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbd_handle_t *``
        - 设备驱动句柄。
      * - ``event``
        - ``usbd_event_t``
        - 要注销的事件，不能为 ``USBD_EVENT_COUNT``。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注销成功。
      * - ``false``
        - 参数非法。

HID类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

HID 设备类驱动在设备核心驱动之上实现 HID 类请求处理，并通过端点完成 HID 报表的收发。应用需填写 ``hidd_handle_t`` 后调用 ``hidd_drv_open()``；通常在 ``USBD_EVENT_ENUM_COMPLETED`` 回调中完成初始化。

.. c:struct:: hidd_handle_t

   HID 设备类驱动句柄。各成员由应用在调用 ``hidd_drv_open()`` 前填写。

   .. list-table::

      * - 成员
        - 类型
        - 必选
        - 说明
      * - ``usbd_handle``
        - ``usbd_handle_t *``
        - 是
        - 关联的设备核心驱动句柄。
      * - ``itf_num``
        - ``uint8_t``
        - 是
        - HID 接口号。
      * - ``in_ep``
        - ``const usb_desc_endpoint_t *``
        - 否
        - IN 端点描述符。设置后驱动打开该端点，用于发送输入报表。
      * - ``out_ep``
        - ``const usb_desc_endpoint_t *``
        - 否
        - OUT 端点描述符。设置后驱动打开该端点，用于接收输出报表。
      * - ``protocol``
        - ``uint8_t``
        - —
        - 当前 HID 协议，取值见 ``hid_protocol_t``。由驱动维护，应用无需初始化。
      * - ``idle_rate``
        - ``uint8_t[256]``
        - —
        - 各报表 ID 对应的空闲速率，由 ``SET_IDLE`` 请求更新，应用无需初始化。
      * - ``ctrl_req_buf``
        - ``uint32_t``
        - —
        - 控制请求响应缓冲区，驱动内部使用。
      * - ``report_buf``
        - ``void *``
        - 否
        - HID 类请求的数据缓冲区。当未提供 ``get_report_prev_cb`` / ``set_report_prev_cb`` 时，驱动使用该缓冲区处理 ``GET_REPORT`` / ``SET_REPORT``。
      * - ``report_buf_size``
        - ``size_t``
        - 否
        - ``report_buf`` 的字节数。
      * - ``get_desc_cb``
        - ``bool (*)(struct hidd_handle *, uint8_t, uint8_t, void **, size_t *)``
        - 是
        - HID 描述符获取回调。``desc_type`` 为 ``HID_DESC_HID``、``HID_DESC_REPORT`` 等，``desc_index`` 为索引。回调需设置 ``*desc`` 和 ``*len``，处理成功返回 ``true``。
      * - ``set_desc_cb``
        - ``bool (*)(struct hidd_handle *, uint8_t, uint8_t, void **, size_t *)``
        - 否
        - HID 描述符设置回调，处理 ``SET_DESCRIPTOR`` 请求。
      * - ``get_report_prev_cb``
        - ``bool (*)(struct hidd_handle *, uint8_t, uint8_t, size_t, size_t *)``
        - 否
        - ``GET_REPORT`` 请求的预处理回调，在建立阶段调用。``type`` 为报表类型，``id`` 为报表 ID，``xfer_len`` 为主机请求长度。回调需通过 ``*len`` 返回有效数据长度，处理成功返回 ``true``。
      * - ``set_report_prev_cb``
        - ``bool (*)(struct hidd_handle *, uint8_t, uint8_t, size_t, size_t *)``
        - 否
        - ``SET_REPORT`` 请求的预处理回调，在建立阶段调用，参数含义同上。
      * - ``get_report_comp_cb``
        - ``void (*)(struct hidd_handle *, uint8_t, uint8_t, void *, size_t)``
        - 否
        - ``GET_REPORT`` 请求完成回调，``buf`` / ``len`` 为发送的数据。
      * - ``set_report_comp_cb``
        - ``void (*)(struct hidd_handle *, uint8_t, uint8_t, void *, size_t)``
        - 否
        - ``SET_REPORT`` 请求完成回调，``buf`` / ``len`` 为接收的数据。
      * - ``set_idle_cb``
        - ``void (*)(struct hidd_handle *, uint8_t, uint8_t)``
        - 否
        - ``SET_IDLE`` 请求处理回调。
      * - ``set_protocol_cb``
        - ``void (*)(struct hidd_handle *, uint8_t)``
        - 否
        - ``SET_PROTOCOL`` 请求处理回调。
      * - ``read_comp_cb``
        - ``void (*)(struct hidd_handle *, void *, size_t)``
        - 否
        - OUT 端点接收完成回调，由 ``hidd_drv_read()`` 发起。
      * - ``write_comp_cb``
        - ``void (*)(struct hidd_handle *, const void *, size_t)``
        - 否
        - IN 端点发送完成回调，由 ``hidd_drv_write()`` 发起。

.. c:function:: bool hidd_drv_open(hidd_handle_t *hidd)

   打开 HID 设备类驱动。驱动注册 HID 接口的控制请求回调，并打开已配置的 IN / OUT 端点。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``hidd``
        - ``hidd_handle_t *``
        - HID 类驱动句柄。``usbd_handle`` 和 ``get_desc_cb`` 必须有效。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 打开成功。
      * - ``false``
        - 参数非法或端点打开失败。

.. c:function:: bool hidd_drv_read(hidd_handle_t *hidd, void *buf, size_t len)

   从 HID 的 OUT 端点发起一次异步读取。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``hidd``
        - ``hidd_handle_t *``
        - HID 类驱动句柄。
      * - ``buf``
        - ``void *``
        - 接收数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 期望读取的最大字节数。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 句柄为空、未配置 ``out_ep`` 或端点忙。

   读取完成后驱动调用 ``read_comp_cb`` 通知应用。回调在 USB 中断上下文中执行。

.. c:function:: bool hidd_drv_write(hidd_handle_t *hidd, const void *buf, size_t len)

   向 HID 的 IN 端点发起一次异步写入。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``hidd``
        - ``hidd_handle_t *``
        - HID 类驱动句柄。
      * - ``buf``
        - ``const void *``
        - 待发送数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 待发送的字节数。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 句柄为空、未配置 ``in_ep`` 或端点忙。

   发送完成后驱动调用 ``write_comp_cb`` 通知应用。回调在 USB 中断上下文中执行。

CDC-ACM类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

CDC-ACM 设备类驱动实现虚拟串口（ACM）的类请求处理和数据收发。应用需填写 ``cdcd_acm_handle_t`` 后调用 ``cdcd_acm_drv_open()``。

.. c:struct:: cdc_line_coding_t

   串口线路参数，对应 CDC ``SET_LINE_CODING`` / ``GET_LINE_CODING`` 请求。

   .. list-table::

      * - 成员
        - 类型
        - 说明
      * - ``bit_rate``
        - ``uint32_t``
        - 波特率，单位 bit/s。
      * - ``stop_bits``
        - ``uint8_t``
        - 停止位。
      * - ``parity``
        - ``uint8_t``
        - 校验方式。
      * - ``data_bits``
        - ``uint8_t``
        - 数据位。

.. c:struct:: cdcd_acm_handle_t

   CDC-ACM 设备类驱动句柄。各成员由应用在调用 ``cdcd_acm_drv_open()`` 前填写。

   .. list-table::

      * - 成员
        - 类型
        - 必选
        - 说明
      * - ``usbd_handle``
        - ``usbd_handle_t *``
        - 是
        - 关联的设备核心驱动句柄。
      * - ``ctrl_itf_num``
        - ``uint8_t``
        - 是
        - 通信控制接口号。
      * - ``data_itf_num``
        - ``uint8_t``
        - 是
        - 数据接口号。
      * - ``notify_ep``
        - ``const usb_desc_endpoint_t *``
        - 是
        - 通知端点描述符，驱动会打开该端点。
      * - ``in_ep``
        - ``const usb_desc_endpoint_t *``
        - 是
        - IN 数据端点描述符，用于设备向主机发送数据。
      * - ``out_ep``
        - ``const usb_desc_endpoint_t *``
        - 是
        - OUT 数据端点描述符，用于设备接收主机数据。
      * - ``line_coding``
        - ``cdc_line_coding_t``
        - —
        - 当前线路参数。收到 ``SET_LINE_CODING`` 请求后由驱动更新。
      * - ``set_line_coding_cb``
        - ``bool (*)(struct cdcd_acm_handle *, cdc_line_coding_t *)``
        - 否
        - ``SET_LINE_CODING`` 请求处理回调，在数据阶段调用。返回 ``true`` 表示接受该参数。
      * - ``set_control_line_state_cb``
        - ``void (*)(struct cdcd_acm_handle *, uint16_t)``
        - 否
        - ``SET_CONTROL_LINE_STATE`` 请求处理回调。``bitmap`` 的 bit0（``CDC_LINE_STATE_DTR_MASK``）和 bit1（``CDC_LINE_STATE_RTS_MASK``）分别表示 DTR / RTS 状态。
      * - ``send_break_cb``
        - ``void (*)(struct cdcd_acm_handle *, uint16_t)``
        - 否
        - ``SEND_BREAK`` 请求处理回调，``duration`` 为 break 持续时间。
      * - ``read_comp_cb``
        - ``void (*)(struct cdcd_acm_handle *, void *, size_t)``
        - 否
        - OUT 端点接收完成回调。
      * - ``write_comp_cb``
        - ``void (*)(struct cdcd_acm_handle *, const void *, size_t)``
        - 否
        - IN 端点发送完成回调。

.. c:function:: bool cdcd_acm_drv_open(cdcd_acm_handle_t *cdcd_acm)

   打开 CDC-ACM 设备类驱动。驱动注册接口控制请求回调，并打开通知端点和数据端点。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cdcd_acm``
        - ``cdcd_acm_handle_t *``
        - CDC-ACM 类驱动句柄。``usbd_handle``、``notify_ep``、``in_ep`` 和 ``out_ep`` 必须有效。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 打开成功。
      * - ``false``
        - 参数非法或端点打开失败。

.. c:function:: bool cdcd_acm_drv_read(cdcd_acm_handle_t *cdcd_acm, void *buf, size_t len)

   从 CDC-ACM 的 OUT 数据端点发起一次异步读取。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cdcd_acm``
        - ``cdcd_acm_handle_t *``
        - CDC-ACM 类驱动句柄。
      * - ``buf``
        - ``void *``
        - 接收数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 期望读取的最大字节数。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 请求提交失败。

   读取完成后驱动调用 ``read_comp_cb`` 通知应用。回调在 USB 中断上下文中执行。

.. c:function:: bool cdcd_acm_drv_write(cdcd_acm_handle_t *cdcd_acm, const void *buf, size_t len)

   向 CDC-ACM 的 IN 数据端点发起一次异步写入。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cdcd_acm``
        - ``cdcd_acm_handle_t *``
        - CDC-ACM 类驱动句柄。
      * - ``buf``
        - ``const void *``
        - 待发送数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 待发送的字节数。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 请求提交失败。

   发送完成后驱动调用 ``write_comp_cb`` 通知应用。回调在 USB 中断上下文中执行。

CDC-ECM类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

CDC-ECM 设备类驱动实现以太网控制模型（Ethernet Control Model），处理数据接口的标准请求和控制接口的以太网类请求，并在数据端点上收发以太网帧。应用需填写 ``cdcd_ecm_handle_t`` 后调用 ``cdcd_ecm_drv_open()``。

驱动处理数据接口的 ``GET_INTERFACE`` / ``SET_INTERFACE`` 标准请求，以及控制接口的 ``SET_ETHERNET_PACKET_FILTER`` 类请求。数据端点不由 ``cdcd_ecm_drv_open()`` 打开，应用通常在 ``set_interface_cb`` 中根据备用设置调用 ``cdcd_ecm_drv_ctrl_data_endpoint()``。

.. c:struct:: cdcd_ecm_handle_t

   CDC-ECM 设备类驱动句柄。各成员由应用在调用 ``cdcd_ecm_drv_open()`` 前填写。

   .. list-table::

      * - 成员
        - 类型
        - 必选
        - 说明
      * - ``usbd_handle``
        - ``usbd_handle_t *``
        - 是
        - 关联的设备核心驱动句柄。
      * - ``ctrl_itf_num``
        - ``uint8_t``
        - 是
        - 通信控制接口号。
      * - ``data_itf_num``
        - ``uint8_t``
        - 是
        - 数据接口号。
      * - ``notify_ep``
        - ``const usb_desc_endpoint_t *``
        - 是
        - 通知端点描述符，驱动打开时会打开该端点。
      * - ``in_ep``
        - ``const usb_desc_endpoint_t *``
        - 是
        - IN 数据端点描述符，用于设备向主机发送以太网帧。
      * - ``out_ep``
        - ``const usb_desc_endpoint_t *``
        - 是
        - OUT 数据端点描述符，用于设备接收主机发来的以太网帧。
      * - ``alt_setting``
        - ``uint8_t``
        - —
        - 数据接口当前的备用设置，由驱动在 ``SET_INTERFACE`` 请求后更新。
      * - ``notify_buf``
        - ``uint8_t[16]``
        - —
        - 通知请求缓冲区，驱动内部使用。
      * - ``set_interface_cb``
        - ``void (*)(struct cdcd_ecm_handle *, uint8_t)``
        - 否
        - ``SET_INTERFACE`` 请求处理回调，``alt_setting`` 为新的备用设置。通常在此打开或关闭数据端点。
      * - ``set_ethernet_packet_filter_cb``
        - ``void (*)(struct cdcd_ecm_handle *, uint16_t)``
        - 否
        - ``SET_ETHERNET_PACKET_FILTER`` 请求处理回调，``bitmap`` 取值见 ``CDC_ECM_ETH_PACK_FILTER_*``。
      * - ``read_comp_cb``
        - ``void (*)(struct cdcd_ecm_handle *, void *, size_t)``
        - 否
        - OUT 数据端点接收完成回调。
      * - ``write_comp_cb``
        - ``void (*)(struct cdcd_ecm_handle *, const void *, size_t)``
        - 否
        - IN 数据端点发送完成回调。

.. c:function:: bool cdcd_ecm_drv_open(cdcd_ecm_handle_t *cdcd_ecm)

   打开 CDC-ECM 设备类驱动。驱动为控制接口和数据接口注册控制请求回调，并打开通知端点。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cdcd_ecm``
        - ``cdcd_ecm_handle_t *``
        - CDC-ECM 类驱动句柄。``usbd_handle``、``notify_ep``、``in_ep`` 和 ``out_ep`` 必须有效。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 打开成功。
      * - ``false``
        - 参数非法或端点打开失败。

.. c:function:: bool cdcd_ecm_drv_ctrl_data_endpoint(cdcd_ecm_handle_t *cdcd_ecm, bool enable)

   打开或关闭 CDC-ECM 的数据端点。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cdcd_ecm``
        - ``cdcd_ecm_handle_t *``
        - CDC-ECM 类驱动句柄。
      * - ``enable``
        - ``bool``
        - 为 ``true`` 时打开 IN / OUT 数据端点，为 ``false`` 时关闭两个端点。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 操作成功。
      * - ``false``
        - 参数非法或端点打开失败。

   打开端点后，驱动通过 ``read_comp_cb`` / ``write_comp_cb`` 通知应用收发完成。回调在 USB 中断上下文中执行。

.. c:function:: bool cdcd_ecm_drv_connect_change_notify(cdcd_ecm_handle_t *cdcd_ecm, bool connected)

   通过通知端点发送网络连接状态变化通知（``CDC_NOTIF_NETWORK_CONNECTION``）。该函数为异步操作。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cdcd_ecm``
        - ``cdcd_ecm_handle_t *``
        - CDC-ECM 类驱动句柄。
      * - ``connected``
        - ``bool``
        - 为 ``true`` 表示网络已连接，为 ``false`` 表示已断开。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 通知提交成功。
      * - ``false``
        - 参数非法或通知端点忙。

.. c:function:: bool cdcd_ecm_drv_speed_change_notify(cdcd_ecm_handle_t *cdcd_ecm, uint32_t ul_bit_rate, uint32_t dl_bit_rate)

   通过通知端点发送连接速度变化通知（``CDC_NOTIF_CONNECTION_SPEED_CHANGE``）。该函数为异步操作。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cdcd_ecm``
        - ``cdcd_ecm_handle_t *``
        - CDC-ECM 类驱动句柄。
      * - ``ul_bit_rate``
        - ``uint32_t``
        - 上行速率，单位 bit/s。
      * - ``dl_bit_rate``
        - ``uint32_t``
        - 下行速率，单位 bit/s。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 通知提交成功。
      * - ``false``
        - 参数非法或通知端点忙。

.. c:function:: bool cdcd_ecm_drv_read(cdcd_ecm_handle_t *cdcd_ecm, void *buf, size_t len)

   从 CDC-ECM 的 OUT 数据端点发起一次异步读取。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cdcd_ecm``
        - ``cdcd_ecm_handle_t *``
        - CDC-ECM 类驱动句柄。
      * - ``buf``
        - ``void *``
        - 接收数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 期望读取的最大字节数。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 请求提交失败。

   读取完成后驱动调用 ``read_comp_cb`` 通知应用。回调在 USB 中断上下文中执行。

.. c:function:: bool cdcd_ecm_drv_write(cdcd_ecm_handle_t *cdcd_ecm, const void *buf, size_t len)

   向 CDC-ECM 的 IN 数据端点发起一次异步写入。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cdcd_ecm``
        - ``cdcd_ecm_handle_t *``
        - CDC-ECM 类驱动句柄。
      * - ``buf``
        - ``const void *``
        - 待发送数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 待发送的字节数。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 请求提交失败。

   发送完成后驱动调用 ``write_comp_cb`` 通知应用。回调在 USB 中断上下文中执行。

UAC类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

UAC 设备类驱动实现 USB 音频类（Audio Class）1.0 的控制请求处理与音频流端点收发。应用需填写 ``uacd_handle_t`` 后调用 ``uacd_drv_open()``，并通过 ``uacd_drv_stream_interface_append()``、``uacd_drv_stream_endpoint_append()`` 为音频流接口和端点补充注册控制请求回调。

驱动不解析音频描述符，也不主动打开流端点。应用通常在 ``SET_INTERFACE`` 回调中根据备用设置打开或关闭流端点，并启动音频数据的异步读写。

.. c:struct:: uacd_handle_t

   UAC 设备类驱动句柄。各成员由应用在调用 ``uacd_drv_open()`` 前填写。

   .. list-table::

      * - 成员
        - 类型
        - 必选
        - 说明
      * - ``usbd_handle``
        - ``usbd_handle_t *``
        - 是
        - 关联的设备核心驱动句柄。
      * - ``ctrl_itf_num``
        - ``uint8_t``
        - 是
        - 音频控制接口号。
      * - ``temp_buf``
        - ``uint8_t[64]``
        - —
        - 类请求数据阶段使用的临时缓冲区，驱动内部使用。
      * - ``set_itf_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t)``
        - 否
        - ``SET_INTERFACE`` 请求处理回调，参数为接口号和新的备用设置。
      * - ``get_itf_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t *)``
        - 否
        - ``GET_INTERFACE`` 请求处理回调，参数为接口号和用于返回当前备用设置的指针。
      * - ``set_cur_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t, uint16_t, void *, size_t)``
        - 否
        - ``SET_CUR`` 类请求状态阶段回调。
      * - ``set_min_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t, uint16_t, void *, size_t)``
        - 否
        - ``SET_MIN`` 类请求状态阶段回调。
      * - ``set_max_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t, uint16_t, void *, size_t)``
        - 否
        - ``SET_MAX`` 类请求状态阶段回调。
      * - ``set_res_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t, uint16_t, void *, size_t)``
        - 否
        - ``SET_RES`` 类请求状态阶段回调。
      * - ``get_cur_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t, uint16_t, void **, size_t *)``
        - 否
        - ``GET_CUR`` 类请求建立阶段回调。
      * - ``get_min_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t, uint16_t, void **, size_t *)``
        - 否
        - ``GET_MIN`` 类请求建立阶段回调。
      * - ``get_max_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t, uint16_t, void **, size_t *)``
        - 否
        - ``GET_MAX`` 类请求建立阶段回调。
      * - ``get_res_cb``
        - ``void (*)(struct uacd_handle *, uint8_t, uint8_t, uint16_t, void **, size_t *)``
        - 否
        - ``GET_RES`` 类请求建立阶段回调。
      * - ``read_comp_cb``
        - ``void (*)(struct uacd_handle *, usb_endp_t, void *, size_t)``
        - 否
        - OUT 流端点接收完成回调，``endp`` 为端点地址。
      * - ``write_comp_cb``
        - ``void (*)(struct uacd_handle *, usb_endp_t, const void *, size_t)``
        - 否
        - IN 流端点发送完成回调，``endp`` 为端点地址。

   ``SET_*`` / ``GET_*`` 回调的 ``rcpt`` 为请求接收方（``USB_REQ_RCPT_INTERFACE`` 或 ``USB_REQ_RCPT_ENDPOINT``）；``id_ep`` 在接收方为接口时是实体 ID（``wIndex`` 高字节），为端点时是端点地址（``wIndex`` 低字节）；``value`` 为 ``wValue``。``GET_*`` 回调需将响应数据指针和长度分别写入 ``*buf``、``*len``，否则驱动将以 STALL 结束请求。

.. c:function:: bool uacd_drv_open(uacd_handle_t *uacd)

   打开 UAC 设备类驱动。驱动为 ``ctrl_itf_num`` 对应的音频控制接口注册控制请求回调，不打开任何端点。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``uacd``
        - ``uacd_handle_t *``
        - UAC 类驱动句柄。``usbd_handle`` 和 ``ctrl_itf_num`` 必须有效。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 打开成功。
      * - ``false``
        - 参数非法或回调注册失败。

.. c:function:: bool uacd_drv_stream_interface_append(uacd_handle_t *uacd, uint8_t itf_num)

   为音频流接口注册控制请求回调，使该接口的 ``GET_INTERFACE`` / ``SET_INTERFACE`` 请求也能由应用处理。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``uacd``
        - ``uacd_handle_t *``
        - UAC 类驱动句柄。
      * - ``itf_num``
        - ``uint8_t``
        - 音频流接口号。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注册成功。
      * - ``false``
        - 参数非法或注册失败。

.. c:function:: bool uacd_drv_stream_endpoint_append(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc)

   为音频流端点注册控制请求回调，用于处理以端点为接收方的类请求，例如设置采样频率。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``uacd``
        - ``uacd_handle_t *``
        - UAC 类驱动句柄。
      * - ``desc``
        - ``const uac_desc_audio_endpoint_t *``
        - 音频流端点描述符，驱动使用其中的 ``bEndpointAddress``。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注册成功。
      * - ``false``
        - 参数非法或注册失败。

.. c:function:: bool uacd_drv_open_stream_endpoint(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc)

   打开音频流端点。IN 端点的发送完成回调为 ``write_comp_cb``，OUT 端点的接收完成回调为 ``read_comp_cb``。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``uacd``
        - ``uacd_handle_t *``
        - UAC 类驱动句柄。
      * - ``desc``
        - ``const uac_desc_audio_endpoint_t *``
        - 待打开的音频流端点描述符。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 打开成功。
      * - ``false``
        - 参数非法或端点打开失败。

.. c:function:: bool uacd_drv_close_stream_endpoint(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc)

   关闭音频流端点。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``uacd``
        - ``uacd_handle_t *``
        - UAC 类驱动句柄。
      * - ``desc``
        - ``const uac_desc_audio_endpoint_t *``
        - 音频流端点描述符。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 关闭成功。
      * - ``false``
        - 参数非法或端点关闭失败。

.. c:function:: bool uacd_drv_read(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc, void *buf, size_t len)

   从 OUT 音频流端点发起一次异步读取，接收主机发来的音频数据。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``uacd``
        - ``uacd_handle_t *``
        - UAC 类驱动句柄。
      * - ``desc``
        - ``const uac_desc_audio_endpoint_t *``
        - 音频流端点描述符。
      * - ``buf``
        - ``void *``
        - 接收数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 期望读取的最大字节数。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 请求提交失败。

   读取完成后驱动调用 ``read_comp_cb`` 通知应用。回调在 USB 中断上下文中执行。

.. c:function:: bool uacd_drv_write(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc, const void *buf, size_t len)

   向 IN 音频流端点发起一次异步写入，向主机发送音频数据。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``uacd``
        - ``uacd_handle_t *``
        - UAC 类驱动句柄。
      * - ``desc``
        - ``const uac_desc_audio_endpoint_t *``
        - 音频流端点描述符。
      * - ``buf``
        - ``const void *``
        - 待发送数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 待发送的字节数。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 请求提交失败。

   发送完成后驱动调用 ``write_comp_cb`` 通知应用。回调在 USB 中断上下文中执行。

主机
----------------------------

核心驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

主机核心驱动负责主机端口状态检测、设备枚举、设备挂起 / 恢复以及传输调度。应用通过板级函数 ``board_usb_init(index, USB_MODE_HOST)`` 获取主机句柄。

.. c:type:: usbh_handle_t

   主机驱动句柄，类型不透明。由 ``board_usb_init()`` 返回。

.. c:type:: usbh_device_t

   已枚举 USB 设备的句柄，类型不透明。该句柄由主机核心驱动内部管理，类驱动通过回调使用，应用无需直接操作。

.. c:function:: bool usbh_drv_open(usbh_handle_t *h)

   打开 USB 主机驱动，使能主机端口。一般调用顺序为：先 ``usbh_drv_open()``，再打开所需的类驱动（``hubh_drv_open()``、``hidh_drv_open()``、``cdch_acm_drv_open()``）。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 打开成功。
      * - ``false``
        - 打开失败。

.. c:function:: void usbh_drv_task(usbh_handle_t *h)

   主机核心驱动任务处理函数，处理端口状态变化、设备枚举、挂起 / 恢复和传输调度。应用需在主循环中周期性调用，且在调用前不要长时间阻塞。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

.. c:function:: bool usbh_drv_close(usbh_handle_t *h)

   关闭 USB 主机驱动，移除已连接设备并关闭主机端口。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 关闭成功。
      * - ``false``
        - 关闭失败。

HUB类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

HUB 类驱动用于支持外部 USB HUB，使主机能够通过 HUB 端口连接多级设备。

.. c:function:: bool hubh_drv_open(usbh_handle_t *h)

   向主机注册 HUB 类驱动。主机枚举到 HUB 设备后，由该驱动管理其下行端口。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注册成功。
      * - ``false``
        - 注册失败。

.. c:function:: void hubh_drv_task(void)

   HUB 类驱动任务处理函数，处理 HUB 枚举和下行端口状态。应用需在主循环中周期性调用。该函数无参数、无返回值。

.. c:function:: bool hubh_drv_close(usbh_handle_t *h)

   注销 HUB 类驱动。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注销成功。
      * - ``false``
        - 注销失败。

HID类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

HID 主机类驱动完成 HID 设备的枚举、报表描述符解析和输入报表读取，并提供键盘、鼠标的通用解析结果。应用通过 ``hidh_drv_open()`` 注册驱动，并在主循环中调用 ``hidh_drv_task()``。键盘和鼠标功能分别通过 ``HIDH_KEYBOARD_EN`` 和 ``HIDH_MOUSE_EN`` 宏使能。

.. c:function:: bool hidh_drv_open(usbh_handle_t *h)

   向主机注册 HID 类驱动。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注册成功。
      * - ``false``
        - 注册失败。

.. c:function:: void hidh_drv_task(void)

   HID 类驱动任务处理函数，处理 HID 设备枚举和输入报表读取。若使能了键盘 / 鼠标功能，该函数还会更新键盘、鼠标状态并触发相应回调。应用需在主循环中周期性调用。该函数无参数、无返回值。

.. c:function:: bool hidh_drv_close(usbh_handle_t *h)

   注销 HID 类驱动。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注销成功。
      * - ``false``
        - 注销失败。

键盘
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. c:struct:: hidh_keyboard_t

   键盘当前状态。

   .. list-table::

      * - 成员
        - 类型
        - 说明
      * - ``led_status``
        - ``uint8_t``
        - LED 状态位图：bit0 为 Num Lock，bit1 为 Caps Lock，bit2 为 Scroll Lock。主机会将该状态发送给键盘。
      * - ``button_status_bitmap``
        - ``uint32_t[256 / 32]``
        - 按键状态位图，以 HID 键盘用法码为位索引，置位表示按下。

.. c:var:: hidh_keyboard_t hidh_keyboard

   键盘全局状态，由 ``hidh_drv_task()`` 更新。多个键盘的状态会合并到该变量中。

.. c:type:: hidh_keyboard_changed_cb

   键盘状态变化回调函数类型。回调由 ``hidh_drv_task()`` 在其任务上下文中调用。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``keyboard``
        - ``hidh_keyboard_t *``
        - 指向全局 ``hidh_keyboard``。

.. c:function:: void hidh_register_keyboard_changed_cb(hidh_keyboard_changed_cb cb)

   注册键盘状态变化回调。重复注册会替换原有回调。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cb``
        - ``hidh_keyboard_changed_cb``
        - 回调函数；传入 ``NULL`` 等价于注销。

.. c:function:: void hidh_unregister_keyboard_changed_cb(void)

   注销键盘状态变化回调。该函数无参数、无返回值。

.. c:function:: bool hidh_keyboard_get_key_status(uint8_t key_code)

   查询指定按键是否按下。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``key_code``
        - ``uint8_t``
        - HID 键盘码，可使用 ``hid_key_code_t`` 中的 ``HID_KEY_CODE_*`` 值。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 按键处于按下状态。
      * - ``false``
        - 按键未按下。

鼠标
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. c:struct:: hidh_mouse_status_t

   鼠标当前状态。

   .. list-table::

      * - 成员
        - 类型
        - 说明
      * - ``buttons``
        - ``uint8_t``
        - 按键位图，bit0 对应按键 1。
      * - ``x``
        - ``uint32_t``
        - 累积的 X 方向位置。初始为 0，按相对位移累加，最小值为 0。
      * - ``y``
        - ``uint32_t``
        - 累积的 Y 方向位置。初始为 0，按相对位移累加，最小值为 0。

.. c:struct:: hidh_mouse_change_t

   本次任务处理周期内的鼠标变化量。

   .. list-table::

      * - 成员
        - 类型
        - 说明
      * - ``dx``
        - ``int32_t``
        - X 方向位移增量。
      * - ``dy``
        - ``int32_t``
        - Y 方向位移增量。
      * - ``wheel``
        - ``int32_t``
        - 滚轮增量。

.. c:var:: hidh_mouse_status_t hidh_mouse_status

   鼠标全局状态，由 ``hidh_drv_task()`` 更新。多个鼠标的状态会合并到该变量中。

.. c:type:: hidh_mouse_changed_cb

   鼠标状态变化回调函数类型。回调由 ``hidh_drv_task()`` 在其任务上下文中调用。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``status``
        - ``hidh_mouse_status_t *``
        - 指向全局 ``hidh_mouse_status``。
      * - ``change``
        - ``hidh_mouse_change_t *``
        - 自上次任务以来的变化量。

.. c:function:: void hidh_register_mouse_changed_cb(hidh_mouse_changed_cb cb)

   注册鼠标状态变化回调。重复注册会替换原有回调。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``cb``
        - ``hidh_mouse_changed_cb``
        - 回调函数；传入 ``NULL`` 等价于注销。

.. c:function:: void hidh_unregister_mouse_changed_cb(void)

   注销鼠标状态变化回调。该函数无参数、无返回值。

CDC-ACM类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

CDC-ACM 主机类驱动实现 USB 虚拟串口主机端。典型使用流程如下：

1. 调用 ``cdch_acm_drv_open()`` 注册类驱动；
2. 在主循环中调用 ``cdch_acm_drv_task()`` 和 ``usbh_drv_task()``；
3. 通过 ``cdch_acm_get_count()`` / ``cdch_acm_get_handle()`` 获取已枚举的 CDC-ACM 实例；
4. 使用 ``cdch_acm_open()`` 配置线路参数并打开实例，然后进行异步读写。

.. c:type:: cdch_acm_handle_t

   CDC-ACM 主机实例句柄，类型不透明。由类驱动在设备枚举完成后创建，应用通过 ``cdch_acm_get_handle()`` 获取。

.. c:type:: cdch_acm_open_cb

   实例打开完成回调函数类型。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``rst``
        - ``bool``
        - 为 ``true`` 表示线路参数设置成功、实例已打开。

.. c:type:: cdch_acm_read_cb

   读取完成回调函数类型。回调在主机任务上下文中执行。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``buf``
        - ``const void *``
        - 本次接收到的数据。
      * - ``len``
        - ``size_t``
        - 本次接收到的字节数。

.. c:type:: cdch_acm_write_cb

   写入完成回调函数类型。回调在主机任务上下文中执行。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``buf``
        - ``const void *``
        - 本次发送的数据。
      * - ``len``
        - ``size_t``
        - 本次发送的字节数。

.. c:function:: bool cdch_acm_drv_open(usbh_handle_t *h)

   向主机注册 CDC-ACM 类驱动。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::
      :widths: 16 84
      :header-rows: 1

      * - 返回值
        - 说明
      * - ``true``
        - 注册成功。
      * - ``false``
        - 注册失败。

.. c:function:: void cdch_acm_drv_task(void)

   CDC-ACM 类驱动任务处理函数，处理实例枚举、线路参数设置、控制信号和读写传输。应用需在主循环中周期性调用。该函数无参数、无返回值。

.. c:function:: bool cdch_acm_drv_close(usbh_handle_t *h)

   注销 CDC-ACM 类驱动。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注销成功。
      * - ``false``
        - 注销失败。

.. c:function:: size_t cdch_acm_get_count(void)

   获取已完成枚举的 CDC-ACM 实例数量。

   .. list-table::

      * - 返回值
        - 说明
      * - 实例数量
        - 已完成枚举的 CDC-ACM 实例数量。
      * - ``0``
        - 未连接设备。

.. c:function:: cdch_acm_handle_t *cdch_acm_get_handle(size_t index)

   按索引获取 CDC-ACM 实例句柄。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``index``
        - ``size_t``
        - 实例索引，从 0 开始。

   .. list-table::

      * - 返回值
        - 说明
      * - 实例句柄
        - 索引有效时返回对应的 ``cdch_acm_handle_t *``。
      * - ``NULL``
        - 索引无效。

.. c:function:: void cdch_acm_open(cdch_acm_handle_t *acm, const cdc_line_coding_t *line_coding, cdch_acm_open_cb cb)

   以指定线路参数打开 CDC-ACM 实例。该函数仅记录请求，实际 ``SET_LINE_CODING`` 请求由 ``cdch_acm_drv_task()`` 异步完成，完成后调用 ``cb``。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``line_coding``
        - ``const cdc_line_coding_t *``
        - 线路参数，函数内部会复制该结构。
      * - ``cb``
        - ``cdch_acm_open_cb``
        - 打开完成回调；参数非法时以 ``rst = false`` 调用。

.. c:function:: void cdch_acm_close(cdch_acm_handle_t *acm)

   关闭 CDC-ACM 实例。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。

.. c:function:: bool cdch_acm_is_open(cdch_acm_handle_t *acm)

   查询 CDC-ACM 实例是否已打开。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 实例已打开。
      * - ``false``
        - 实例未打开。

.. c:function:: bool cdch_acm_read(cdch_acm_handle_t *acm, void *buf, size_t len, uint32_t timeout, cdch_acm_read_cb cb)

   从 CDC-ACM 实例发起一次异步读取。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``buf``
        - ``void *``
        - 接收数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 期望读取的最大字节数，不能为 0。
      * - ``timeout``
        - ``uint32_t``
        - 超时时间，单位为毫秒；为 0 表示不启用超时。
      * - ``cb``
        - ``cdch_acm_read_cb``
        - 读取完成回调。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 实例未打开、参数非法或端点忙。

   ``cb`` 在传输成功时由类驱动调用。

.. c:function:: bool cdch_acm_write(cdch_acm_handle_t *acm, const void *buf, size_t len, uint32_t timeout, cdch_acm_write_cb cb)

   向 CDC-ACM 实例发起一次异步写入。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``buf``
        - ``const void *``
        - 待发送数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 待发送的字节数，不能为 0。
      * - ``timeout``
        - ``uint32_t``
        - 超时时间，单位为毫秒；为 0 表示不启用超时。
      * - ``cb``
        - ``cdch_acm_write_cb``
        - 写入完成回调。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 实例未打开、参数非法或端点忙。

   ``cb`` 在传输成功时由类驱动调用。

.. c:function:: void cdch_acm_set_dtr(cdch_acm_handle_t *acm, bool dtr)

   设置 DTR 信号。该状态与 RTS 一起在下一次 ``cdch_acm_drv_task()`` 中通过 ``SET_CONTROL_LINE_STATE`` 请求发送。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``dtr``
        - ``bool``
        - 为 ``true`` 时置位 DTR，为 ``false`` 时清除。

.. c:function:: void cdch_acm_set_rts(cdch_acm_handle_t *acm, bool rts)

   设置 RTS 信号。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``rts``
        - ``bool``
        - 为 ``true`` 时置位 RTS，为 ``false`` 时清除。

.. c:function:: void cdch_acm_set_break(cdch_acm_handle_t *acm, bool brk)

   设置 break 状态。为 ``true`` 时发送持续时间为 ``0xFFFF`` 的 break，为 ``false`` 时发送持续时间为 0 的 break。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``brk``
        - ``bool``
        - break 状态。

.. c:function:: void cdch_acm_send_break(cdch_acm_handle_t *acm, uint16_t duration)

   发送指定持续时间的 break 信号。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``duration``
        - ``uint16_t``
        - break 持续时间。

.. c:function:: bool cdch_acm_get_dtr(cdch_acm_handle_t *acm)

   获取当前 DTR 状态。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - DTR 已置位。
      * - ``false``
        - DTR 未置位。

.. c:function:: bool cdch_acm_get_rts(cdch_acm_handle_t *acm)

   获取当前 RTS 状态。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``cdch_acm_handle_t *``
        - CDC-ACM 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - RTS 已置位。
      * - ``false``
        - RTS 未置位。

CDC-ECM类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

CDC-ECM 主机类驱动实现以太网控制模型的主机端。典型使用流程如下：

1. 调用 ``cdch_ecm_drv_open()`` 注册类驱动；
2. 在主循环中调用 ``cdch_ecm_drv_task()`` 和 ``usbh_drv_task()``；
3. 通过 ``cdch_ecm_get_count()`` / ``cdch_ecm_get_handle()`` 获取已枚举的 CDC-ECM 实例；
4. 调用 ``cdch_ecm_open()`` 打开实例，然后进行异步读写。

驱动按接口类 ``USB_CLASS_CDC`` 和子类 ``CDC_SUBCLASS_ETHERNET_CONTROL_MODEL`` 匹配设备。绑定后驱动依次读取 MAC 地址字符串描述符、将数据接口切换到关闭状态并设置以太网包过滤器；打开实例时再切换到数据接口的启用设置并打开通知、IN 和 OUT 端点。

.. c:type:: cdch_ecm_handle_t

   CDC-ECM 主机实例句柄，类型不透明。由类驱动在设备枚举完成后创建，应用通过 ``cdch_ecm_get_handle()`` 获取。

.. c:type:: cdch_ecm_open_cb

   实例打开完成回调函数类型。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。
      * - ``rst``
        - ``bool``
        - 为 ``true`` 表示数据接口切换和端点打开成功、实例已打开。

.. c:type:: cdch_ecm_connect_change_cb

   网络连接状态变化回调函数类型，由设备的网络连接状态通知触发。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。
      * - ``connected``
        - ``bool``
        - 为 ``true`` 表示网络已连接。

.. c:type:: cdch_ecm_speed_change_cb

   连接速度变化回调函数类型，由设备的连接速度变化通知触发。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。
      * - ``upload_speed``
        - ``uint32_t``
        - 上行速率，单位 bit/s。
      * - ``download_speed``
        - ``uint32_t``
        - 下行速率，单位 bit/s。

.. c:type:: cdch_ecm_read_cb

   读取完成回调函数类型。回调在主机任务上下文中执行。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。
      * - ``buf``
        - ``const void *``
        - 本次接收到的以太网帧。
      * - ``len``
        - ``size_t``
        - 本次接收到的字节数。

.. c:type:: cdch_ecm_write_cb

   写入完成回调函数类型。回调在主机任务上下文中执行。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。
      * - ``buf``
        - ``const void *``
        - 本次发送的以太网帧。
      * - ``len``
        - ``size_t``
        - 本次发送的字节数。

.. c:function:: bool cdch_ecm_drv_open(usbh_handle_t *h)

   向主机注册 CDC-ECM 类驱动。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注册成功。
      * - ``false``
        - 注册失败。

.. c:function:: void cdch_ecm_drv_task(void)

   CDC-ECM 类驱动任务处理函数，处理设备枚举、实例打开 / 关闭、通知读取和数据传输。应用需在主循环中周期性调用。该函数无参数、无返回值。

.. c:function:: bool cdch_ecm_drv_close(usbh_handle_t *h)

   注销 CDC-ECM 类驱动。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注销成功。
      * - ``false``
        - 注销失败。

.. c:function:: size_t cdch_ecm_get_count(void)

   获取已完成枚举的 CDC-ECM 实例数量。

   .. list-table::

      * - 返回值
        - 说明
      * - 实例数量
        - 已完成枚举的 CDC-ECM 实例数量。
      * - ``0``
        - 未连接设备。

.. c:function:: cdch_ecm_handle_t *cdch_ecm_get_handle(size_t index)

   按索引获取 CDC-ECM 实例句柄。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``index``
        - ``size_t``
        - 实例索引，从 0 开始。

   .. list-table::

      * - 返回值
        - 说明
      * - 实例句柄
        - 索引有效时返回对应的 ``cdch_ecm_handle_t *``。
      * - ``NULL``
        - 索引无效。

.. c:function:: void cdch_ecm_open(cdch_ecm_handle_t *ecm, cdch_ecm_open_cb open_cb, cdch_ecm_connect_change_cb connect_cb, cdch_ecm_speed_change_cb speed_cb)

   打开 CDC-ECM 实例。该函数仅记录目标状态和回调，实际数据接口切换与端点打开由 ``cdch_ecm_drv_task()`` 异步完成，完成后调用 ``open_cb``。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。
      * - ``open_cb``
        - ``cdch_ecm_open_cb``
        - 打开完成回调；``ecm`` 为空时以 ``rst = false`` 调用。
      * - ``connect_cb``
        - ``cdch_ecm_connect_change_cb``
        - 网络连接状态变化回调。
      * - ``speed_cb``
        - ``cdch_ecm_speed_change_cb``
        - 连接速度变化回调。

.. c:function:: void cdch_ecm_close(cdch_ecm_handle_t *ecm)

   关闭 CDC-ECM 实例。该函数仅记录目标状态，实际接口切换和端点关闭由 ``cdch_ecm_drv_task()`` 异步完成。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。

.. c:function:: const uint8_t *cdch_ecm_get_mac_addr(cdch_ecm_handle_t *ecm)

   获取实例的 MAC 地址。该地址由设备提供的 MAC 地址字符串描述符解析得到。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - MAC 地址指针
        - 指向 6 字节 MAC 地址的指针。
      * - ``NULL``
        - 句柄为空。

.. c:function:: bool cdch_ecm_is_open(cdch_ecm_handle_t *ecm)

   查询 CDC-ECM 实例是否已打开。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 实例已打开。
      * - ``false``
        - 实例未打开。

.. c:function:: bool cdch_ecm_is_connected(cdch_ecm_handle_t *ecm)

   查询 CDC-ECM 实例的网络连接状态。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 网络已连接。
      * - ``false``
        - 网络未连接。

.. c:function:: bool cdch_ecm_get_speed(cdch_ecm_handle_t *ecm, uint32_t *upload_speed, uint32_t *download_speed)

   获取最近一次连接速度变化通知中的速率。未收到通知前，速率为 0。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。
      * - ``upload_speed``
        - ``uint32_t *``
        - 输出参数，上行速率，单位 bit/s。
      * - ``download_speed``
        - ``uint32_t *``
        - 输出参数，下行速率，单位 bit/s。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 获取成功。
      * - ``false``
        - 参数为空。

.. c:function:: bool cdch_ecm_read(cdch_ecm_handle_t *ecm, void *buf, size_t len, uint32_t timeout, cdch_ecm_read_cb cb)

   从 CDC-ECM 实例发起一次异步读取，接收设备发来的以太网帧。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。
      * - ``buf``
        - ``void *``
        - 接收数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 期望读取的最大字节数，不能为 0。
      * - ``timeout``
        - ``uint32_t``
        - 超时时间，单位为毫秒；为 0 表示不启用超时。
      * - ``cb``
        - ``cdch_ecm_read_cb``
        - 读取完成回调。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 实例未打开、网络未连接、参数非法或端点忙。

   ``cb`` 在传输成功时由类驱动调用。

.. c:function:: bool cdch_ecm_write(cdch_ecm_handle_t *ecm, const void *buf, size_t len, uint32_t timeout, cdch_ecm_write_cb cb)

   向 CDC-ECM 实例发起一次异步写入，发送以太网帧。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``ecm``
        - ``cdch_ecm_handle_t *``
        - CDC-ECM 实例句柄。
      * - ``buf``
        - ``const void *``
        - 待发送数据的缓冲区。
      * - ``len``
        - ``size_t``
        - 待发送的字节数，不能为 0。
      * - ``timeout``
        - ``uint32_t``
        - 超时时间，单位为毫秒；为 0 表示不启用超时。
      * - ``cb``
        - ``cdch_ecm_write_cb``
        - 写入完成回调。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求提交成功。
      * - ``false``
        - 实例未打开、网络未连接、参数非法或端点忙。

   ``cb`` 在传输成功时由类驱动调用。若发送长度恰为端点最大包长的整数倍，驱动会自动追加零长度包，回调中的 ``len`` 仍为原始长度。

UAC类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

UAC 主机类驱动实现 USB 音频类设备的主机端，完成音频控制接口和音频流接口的枚举与解析，并提供音频流的异步打开、关闭和数据传输。典型使用流程如下：

1. 调用 ``uach_drv_open()`` 注册类驱动；
2. 在主循环中调用 ``uach_drv_task()`` 和 ``usbh_drv_task()``；
3. 通过 ``uach_get_count()`` / ``uach_get_handle()`` 获取已枚举的 UAC 实例；
4. 通过 ``uach_get_stream_count()`` / ``uach_get_stream_handle()`` 遍历音频流接口，调用 ``uach_stream_get_info()`` 查询流的当前状态，调用 ``uach_stream_get_alt_info()`` 遍历各备用设置的格式与端点信息；
5. 调用 ``uach_stream_set_alt_setting()`` 切换到选中的备用设置，端点支持频率控制时再调用 ``uach_stream_set_sample_rate()`` 设置采样率；
6. 流就绪后调用 ``uach_stream_capture()`` / ``uach_stream_playback()`` 按包收发音频数据；关闭流时将备用设置切换回 0。

驱动按接口类 ``USB_CLASS_AUDIO`` 和子类 ``UAC_SUBCLASS_AUDIO_CONTROL`` 匹配设备，仅支持全速和高速设备以及 UAC 1.0（``bcdADC`` 为 ``0x0100``）。绑定设备时驱动解析并校验 AudioControl 拓扑与各音频流接口的描述符：音频流接口必须提供无端点的备用设置 0；非零备用设置必须包含等时数据端点及 AS 通用描述符、格式类型描述符和类特定端点描述符，可选的同步端点必须为等时端点并被数据端点的 ``bSynchAddress`` 引用。驱动仅支持 Type I 和 Type III 格式，Type II 备用设置会被跳过。流方向由 ``bTerminalLink`` 关联的 USB Streaming 端子决定。绑定完成后，驱动在任务中依次将各音频流接口切换到备用设置 0，此后实例才可通过 ``uach_get_count()`` / ``uach_get_handle()`` 获取；描述符校验失败时驱动放弃绑定，枚举控制传输失败时自动解除绑定。

.. note::

   切换备用设置、设置采样率以及数据包传输均为异步操作：接口调用仅提交请求，由 ``uach_drv_task()`` 推进，完成时在主机任务上下文中调用回调。同一音频流同一时间只允许一个控制请求，请求处理期间再次调用 ``uach_stream_set_alt_setting()`` 或 ``uach_stream_set_sample_rate()`` 会返回 ``false``。

.. c:type:: uach_handle_t

   UAC 主机实例句柄，类型不透明。由类驱动在设备枚举完成后创建，应用通过 ``uach_get_handle()`` 获取。

.. c:type:: uach_stream_handle_t

   音频流接口句柄，类型不透明。由类驱动在绑定设备时创建，应用通过 ``uach_get_stream_handle()`` 获取；对应 UAC 实例解除绑定后该句柄失效。

.. c:enum:: uach_stream_dir_t

   音频流方向。

   .. list-table::

      * - 取值
        - 数值
        - 说明
      * - ``UACH_STREAM_DIR_CAPTURE``
        - 0
        - 采集流，数据从设备发往主机。
      * - ``UACH_STREAM_DIR_PLAYBACK``
        - 1
        - 播放流，数据从主机发往设备。

.. c:struct:: uach_stream_info_t

   音频流接口的当前信息，由 ``uach_stream_get_info()`` 填充。

   .. list-table::

      * - 成员
        - 类型
        - 说明
      * - ``itf_num``
        - ``uint8_t``
        - 音频流接口号。
      * - ``dir``
        - ``uint8_t``
        - 流方向，取值见 ``uach_stream_dir_t``。
      * - ``terminal_link``
        - ``uint8_t``
        - 流关联的 USB Streaming 端子 ID。
      * - ``alt_count``
        - ``uint8_t``
        - 可用的非零备用设置数量，受 ``UACH_MAX_STREAM_ALT_SETTING_COUNT`` 限制，也是 ``uach_stream_get_alt_info()`` 的索引上限。
      * - ``cur_alt``
        - ``uint8_t``
        - 当前生效的备用设置；为 0 表示流处于空闲（零带宽）状态。
      * - ``channel_config``
        - ``uint16_t``
        - 关联端子的声道空间位置位图。
      * - ``cur_sample_freq``
        - ``uint32_t``
        - 当前采样率，单位 Hz；为 0 表示未知。

.. c:struct:: uach_stream_alt_info_t

   音频流接口某个非零备用设置的详细信息，由 ``uach_stream_get_alt_info()`` 填充。

   .. list-table::

      * - 成员
        - 类型
        - 说明
      * - ``alt_setting``
        - ``uint8_t``
        - 该备用设置的 ``bAlternateSetting``，用于调用 ``uach_stream_set_alt_setting()``。
      * - ``delay``
        - ``uint8_t``
        - AS 通用描述符中的 ``bDelay``，单位帧。
      * - ``format_type``
        - ``uint8_t``
        - 格式类型，取值见 ``uac_format_type_t``。驱动仅接受 ``UAC_FORMAT_TYPE_I`` 和 ``UAC_FORMAT_TYPE_III``。
      * - ``channels``
        - ``uint8_t``
        - 声道数，即 ``bNrChannels``。
      * - ``sub_frame_size``
        - ``uint8_t``
        - 每个声道每个采样的字节数，即 ``bSubframeSize``。
      * - ``bit_resolution``
        - ``uint8_t``
        - 采样位深，单位 bit。
      * - ``sample_freq_type``
        - ``uint8_t``
        - 采样率类型，即 ``bSamFreqType``。为 0 表示连续范围，此时使用 ``min_sample_freq`` / ``max_sample_freq``；大于 0 表示离散采样率的个数，此时使用 ``sample_freq``。
      * - ``sample_freq_count``
        - ``uint8_t``
        - ``sample_freq`` 中的有效条目数，不超过 ``UACH_MAX_SAMPLE_FREQ`` （8）。
      * - ``endp_addr``
        - ``uint8_t``
        - 等时数据端点的地址。
      * - ``sync_endp_addr``
        - ``uint8_t``
        - 等时同步端点的地址，没有同步端点时为 0。
      * - ``sync_type``
        - ``uint8_t``
        - 数据端点的同步类型，取自端点 ``bmAttributes`` 的同步类型位，取值见 ``UAC_ENDP_ATTR_SYNC_TYPE_*``。
      * - ``interval``
        - ``uint8_t``
        - 数据端点的 ``bInterval``。
      * - ``lock_delay_units``
        - ``uint8_t``
        - 类特定端点描述符中的 ``bLockDelayUnits``。
      * - ``lock_delay``
        - ``uint16_t``
        - 类特定端点描述符中的 ``wLockDelay``。
      * - ``max_packet_size``
        - ``uint16_t``
        - 数据端点的最大包长，取自 ``wMaxPacketSize``。
      * - ``format_tag``
        - ``uint16_t``
        - 音频格式标签，PCM 为 ``UAC_FORMAT_TAG_TYPE_I_PCM`` （``0x0001``）。
      * - ``freq_ctrl``
        - ``bool``
        - 数据端点是否支持采样率控制。
      * - ``pitch_ctrl``
        - ``bool``
        - 数据端点是否支持音高控制。
      * - ``max_packets_only``
        - ``bool``
        - 数据端点是否只接受最大包长的数据包。
      * - ``min_sample_freq``
        - ``uint32_t``
        - 最小采样率，仅 ``sample_freq_type`` 为 0 时有效。
      * - ``max_sample_freq``
        - ``uint32_t``
        - 最大采样率，仅 ``sample_freq_type`` 为 0 时有效。
      * - ``sample_freq``
        - ``uint32_t[UACH_MAX_SAMPLE_FREQ]``
        - 离散采样率列表，仅 ``sample_freq_type`` 大于 0 时有效。

.. c:type:: uach_stream_ctrl_cb

   流控制完成回调函数类型，切换备用设置或设置采样率完成后调用，在主机任务上下文中执行。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``stream``
        - ``uach_stream_handle_t *``
        - 音频流接口句柄。
      * - ``rst``
        - ``bool``
        - 为 ``true`` 表示请求成功完成。

.. c:type:: uach_stream_xfer_cb

   流数据包传输完成回调函数类型，仅在传输成功时调用，在主机任务上下文中执行。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``stream``
        - ``uach_stream_handle_t *``
        - 音频流接口句柄。
      * - ``buf``
        - ``void *``
        - 本次传输使用的缓冲区。
      * - ``len``
        - ``size_t``
        - 本次传输的字节数。

.. c:function:: void uach_drv_task(void)

   UAC 类驱动任务处理函数，处理音频流接口的初始切换、备用设置切换和采样率设置。应用需在主循环中周期性调用。该函数无参数、无返回值。

.. c:function:: bool uach_drv_open(usbh_handle_t *h)

   向主机注册 UAC 类驱动。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注册成功。
      * - ``false``
        - 注册失败。

.. c:function:: bool uach_drv_close(usbh_handle_t *h)

   注销 UAC 类驱动。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``h``
        - ``usbh_handle_t *``
        - 主机驱动句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 注销成功。
      * - ``false``
        - 注销失败。

.. c:function:: size_t uach_get_count(void)

   获取已完成枚举的 UAC 实例数量。

   .. list-table::

      * - 返回值
        - 说明
      * - 实例数量
        - 已完成枚举的 UAC 实例数量。
      * - ``0``
        - 未连接设备。

.. c:function:: uach_handle_t *uach_get_handle(size_t index)

   按索引获取 UAC 实例句柄。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``index``
        - ``size_t``
        - 实例索引，从 0 开始。

   .. list-table::

      * - 返回值
        - 说明
      * - 实例句柄
        - 索引有效时返回对应的 ``uach_handle_t *``。
      * - ``NULL``
        - 索引无效。

.. c:function:: size_t uach_get_stream_count(uach_handle_t *uac)

   获取 UAC 实例中可用音频流接口的数量。缺少受支持格式的音频流接口已被驱动在绑定阶段跳过，不计入其中。数量受 ``UACH_MAX_STREAM_ITF_COUNT`` 限制。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``uac``
        - ``uach_handle_t *``
        - UAC 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - 数量
        - 可用的音频流接口数量。
      * - ``0``
        - 句柄为空或实例尚未完成枚举。

.. c:function:: uach_stream_handle_t *uach_get_stream_handle(uach_handle_t *uac, size_t index)

   按索引获取音频流接口句柄。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``uac``
        - ``uach_handle_t *``
        - UAC 实例句柄。
      * - ``index``
        - ``size_t``
        - 音频流接口索引，从 0 开始，应小于 ``uach_get_stream_count()`` 的返回值。

   .. list-table::

      * - 返回值
        - 说明
      * - 句柄
        - 索引有效时返回对应的 ``uach_stream_handle_t *``。
      * - ``NULL``
        - 句柄为空、实例尚未完成枚举或索引无效。

.. c:function:: bool uach_stream_get_info(uach_stream_handle_t *stream, uach_stream_info_t *info)

   获取音频流接口的当前信息，包括当前生效的备用设置和采样率。驱动先清空 ``info`` 再填充；应用可通过 ``cur_alt`` 是否为 0 判断流是否已打开。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``stream``
        - ``uach_stream_handle_t *``
        - 音频流接口句柄。
      * - ``info``
        - ``uach_stream_info_t *``
        - 用于接收音频流信息的结构体。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 获取成功。
      * - ``false``
        - 参数为空。

.. c:function:: bool uach_stream_get_alt_info(uach_stream_handle_t *stream, size_t index, uach_stream_alt_info_t *info)

   获取音频流接口某个非零备用设置的详细信息。``index`` 的有效范围为 ``[0, uach_stream_info_t.alt_count)``，信息由驱动从设备描述符解析得到，切换备用设置后保持不变。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``stream``
        - ``uach_stream_handle_t *``
        - 音频流接口句柄。
      * - ``index``
        - ``size_t``
        - 备用设置索引，从 0 开始。
      * - ``info``
        - ``uach_stream_alt_info_t *``
        - 用于接收备用设置信息的结构体。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 获取成功。
      * - ``false``
        - 参数为空或索引超出 ``alt_count``。

.. c:function:: bool uach_stream_set_alt_setting(uach_stream_handle_t *stream, uint8_t alt_setting, uach_stream_ctrl_cb cb)

   请求切换音频流接口的备用设置。``alt_setting`` 为 0 时关闭流，为非零时切换到对应备用设置并在切换成功后打开数据端点。该函数仅提交请求，实际的 ``SET_INTERFACE`` 请求由 ``uach_drv_task()`` 异步发送，完成后调用 ``cb``。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``stream``
        - ``uach_stream_handle_t *``
        - 音频流接口句柄。
      * - ``alt_setting``
        - ``uint8_t``
        - 目标备用设置，取值为 0 或 ``uach_stream_alt_info_t.alt_setting``。
      * - ``cb``
        - ``uach_stream_ctrl_cb``
        - 完成回调，可为 ``NULL``。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求已提交。
      * - ``false``
        - 句柄为空、备用设置无效或同一流已有控制请求在处理。

   若非零备用设置切换成功但数据端点打开失败，驱动会自动将接口回滚到备用设置 0，并以 ``rst = false`` 调用 ``cb``。切换成功后，若格式描述符只声明一个离散采样率，``uach_stream_info_t.cur_sample_freq`` 会被设为该采样率；否则在调用 ``uach_stream_set_sample_rate()`` 成功前保持为 0。

.. c:function:: bool uach_stream_set_sample_rate(uach_stream_handle_t *stream, uint32_t sample_rate, uach_stream_ctrl_cb cb)

   请求设置当前备用设置下的采样率。仅当数据端点在类特定端点描述符中声明支持采样率控制（``uach_stream_alt_info_t.freq_ctrl`` 为 ``true``）时可用，且 ``sample_rate`` 必须落在格式描述符声明的连续范围内或等于某个离散采样率。该函数仅提交请求，实际的类特定 ``SET_CUR`` 请求由 ``uach_drv_task()`` 异步发送，完成后调用 ``cb``。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``stream``
        - ``uach_stream_handle_t *``
        - 音频流接口句柄。
      * - ``sample_rate``
        - ``uint32_t``
        - 目标采样率，单位 Hz。
      * - ``cb``
        - ``uach_stream_ctrl_cb``
        - 完成回调，可为 ``NULL``。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求已提交。
      * - ``false``
        - 句柄为空、流未处于非零备用设置、端点不支持采样率控制、采样率不受支持或同一流已有控制请求在处理。

.. c:function:: bool uach_stream_capture(uach_stream_handle_t *stream, void *buf, size_t size, uach_stream_xfer_cb cb)

   在当前备用设置的采集流上发起一次等时数据包读取，接收设备发往主机的音频数据。该函数仅提交请求，接收完成后调用 ``cb``；传输失败时不会调用回调。传输不设置超时。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``stream``
        - ``uach_stream_handle_t *``
        - 音频流接口句柄。
      * - ``buf``
        - ``void *``
        - 接收数据的缓冲区。
      * - ``size``
        - ``size_t``
        - 缓冲区大小，不能小于数据端点的最大包长（``uach_stream_alt_info_t.max_packet_size``）。
      * - ``cb``
        - ``uach_stream_xfer_cb``
        - 传输完成回调，可为 ``NULL``。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求已提交。
      * - ``false``
        - 参数非法、流未打开（备用设置为 0）、流方向不是采集、同一流有控制请求在处理或端点忙。

   应用可在 ``cb`` 中再次调用本接口实现连续采集。

.. c:function:: bool uach_stream_playback(uach_stream_handle_t *stream, const void *buf, size_t size, uach_stream_xfer_cb cb)

   在当前备用设置的播放流上发起一次等时数据包写入，向设备发送主机发来的音频数据。该函数仅提交请求，发送完成后调用 ``cb``；传输失败时不会调用回调。传输不设置超时。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``stream``
        - ``uach_stream_handle_t *``
        - 音频流接口句柄。
      * - ``buf``
        - ``const void *``
        - 待发送数据的缓冲区。
      * - ``size``
        - ``size_t``
        - 待发送的字节数，不能为 0 且不能超过数据端点的最大包长（``uach_stream_alt_info_t.max_packet_size``）。
      * - ``cb``
        - ``uach_stream_xfer_cb``
        - 传输完成回调，可为 ``NULL``。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 请求已提交。
      * - ``false``
        - 参数非法、流未打开（备用设置为 0）、流方向不是播放、同一流有控制请求在处理或端点忙。

   应用可在 ``cb`` 中再次调用本接口实现连续播放。
