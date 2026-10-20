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
        - ``bool (*)(hidd_handle_t *, uint8_t, uint8_t, void **, size_t *)``
        - 是
        - HID 描述符获取回调。``desc_type`` 为 ``HID_DESC_HID``、``HID_DESC_REPORT`` 等，``desc_index`` 为索引。回调需设置 ``*desc`` 和 ``*len``，处理成功返回 ``true``。
      * - ``set_desc_cb``
        - ``bool (*)(hidd_handle_t *, uint8_t, uint8_t, void **, size_t *)``
        - 否
        - HID 描述符设置回调，处理 ``SET_DESCRIPTOR`` 请求。
      * - ``get_report_prev_cb``
        - ``bool (*)(hidd_handle_t *, uint8_t, uint8_t, size_t, size_t *)``
        - 否
        - ``GET_REPORT`` 请求的预处理回调，在建立阶段调用。``type`` 为报表类型，``id`` 为报表 ID，``xfer_len`` 为主机请求长度。回调需通过 ``*len`` 返回有效数据长度，处理成功返回 ``true``。
      * - ``set_report_prev_cb``
        - ``bool (*)(hidd_handle_t *, uint8_t, uint8_t, size_t, size_t *)``
        - 否
        - ``SET_REPORT`` 请求的预处理回调，在建立阶段调用，参数含义同上。
      * - ``get_report_comp_cb``
        - ``void (*)(hidd_handle_t *, uint8_t, uint8_t, void *, size_t)``
        - 否
        - ``GET_REPORT`` 请求完成回调，``buf`` / ``len`` 为发送的数据。
      * - ``set_report_comp_cb``
        - ``void (*)(hidd_handle_t *, uint8_t, uint8_t, void *, size_t)``
        - 否
        - ``SET_REPORT`` 请求完成回调，``buf`` / ``len`` 为接收的数据。
      * - ``set_idle_cb``
        - ``void (*)(hidd_handle_t *, uint8_t, uint8_t)``
        - 否
        - ``SET_IDLE`` 请求处理回调。
      * - ``set_protocol_cb``
        - ``void (*)(hidd_handle_t *, uint8_t)``
        - 否
        - ``SET_PROTOCOL`` 请求处理回调。
      * - ``read_comp_cb``
        - ``void (*)(hidd_handle_t *, void *, size_t)``
        - 否
        - OUT 端点接收完成回调，由 ``hidd_drv_read()`` 发起。
      * - ``write_comp_cb``
        - ``void (*)(hidd_handle_t *, const void *, size_t)``
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

CDC类驱动
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
        - ``bool (*)(cdcd_acm_handle_t *, cdc_line_coding_t *)``
        - 否
        - ``SET_LINE_CODING`` 请求处理回调，在数据阶段调用。返回 ``true`` 表示接受该参数。
      * - ``set_control_line_state_cb``
        - ``void (*)(cdcd_acm_handle_t *, uint16_t)``
        - 否
        - ``SET_CONTROL_LINE_STATE`` 请求处理回调。``bitmap`` 的 bit0（``CDC_LINE_STATE_DTR_MASK``）和 bit1（``CDC_LINE_STATE_RTS_MASK``）分别表示 DTR / RTS 状态。
      * - ``send_break_cb``
        - ``void (*)(cdcd_acm_handle_t *, uint16_t)``
        - 否
        - ``SEND_BREAK`` 请求处理回调，``duration`` 为 break 持续时间。
      * - ``read_comp_cb``
        - ``void (*)(cdcd_acm_handle_t *, void *, size_t)``
        - 否
        - OUT 端点接收完成回调。
      * - ``write_comp_cb``
        - ``void (*)(cdcd_acm_handle_t *, const void *, size_t)``
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

HID 主机类驱动完成 HID 设备的枚举、报表描述符解析和输入报表读取，并提供键盘、鼠标的通用解析结果。应用通过 ``hidh_drv_open()`` 注册驱动，并在主循环中调用 ``hidh_drv_task()``。键盘和鼠标功能分别通过 ``USB_CLASS_HIDH_KEYBOARD_EN`` 和 ``USB_CLASS_HIDH_MOUSE_EN`` 宏使能。

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

CDC类驱动
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

CDC-ACM 主机类驱动实现 USB 虚拟串口主机端。典型使用流程如下：

1. 调用 ``cdch_acm_drv_open()`` 注册类驱动；
2. 在主循环中调用 ``cdch_acm_drv_task()`` 和 ``usbh_drv_task()``；
3. 通过 ``cdch_acm_get_count()`` / ``cdch_acm_get_handle()`` 获取已枚举的 CDC-ACM 实例；
4. 使用 ``cdch_acm_open()`` 配置线路参数并打开实例，然后进行异步读写。

.. c:type:: acmh_handle_t

   CDC-ACM 主机实例句柄，类型不透明。由类驱动在设备枚举完成后创建，应用通过 ``cdch_acm_get_handle()`` 获取。

.. c:type:: cdch_acm_open_cb

   实例打开完成回调函数类型。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
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
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``buf``
        - ``const void *``
        - 本次接收到的数据。
      * - ``length``
        - ``size_t``
        - 本次接收到的字节数。

.. c:type:: cdch_acm_write_cb

   写入完成回调函数类型。回调在主机任务上下文中执行。无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``buf``
        - ``const void *``
        - 本次发送的数据。
      * - ``length``
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

.. c:function:: acmh_handle_t *cdch_acm_get_handle(size_t index)

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
        - 索引有效时返回对应的 ``acmh_handle_t *``。
      * - ``NULL``
        - 索引无效。

.. c:function:: void cdch_acm_open(acmh_handle_t *acm, const cdc_line_coding_t *line_coding, cdch_acm_open_cb cb)

   以指定线路参数打开 CDC-ACM 实例。该函数仅记录请求，实际 ``SET_LINE_CODING`` 请求由 ``cdch_acm_drv_task()`` 异步完成，完成后调用 ``cb``。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``line_coding``
        - ``const cdc_line_coding_t *``
        - 线路参数，函数内部会复制该结构。
      * - ``cb``
        - ``cdch_acm_open_cb``
        - 打开完成回调；参数非法时以 ``rst = false`` 调用。

.. c:function:: void cdch_acm_close(acmh_handle_t *acm)

   关闭 CDC-ACM 实例。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。

.. c:function:: bool cdch_acm_is_open(acmh_handle_t *acm)

   查询 CDC-ACM 实例是否已打开。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - 实例已打开。
      * - ``false``
        - 实例未打开。

.. c:function:: bool cdch_acm_read(acmh_handle_t *acm, void *buf, size_t length, uint32_t timeout, cdch_acm_read_cb cb)

   从 CDC-ACM 实例发起一次异步读取。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``buf``
        - ``void *``
        - 接收数据的缓冲区。
      * - ``length``
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

.. c:function:: bool cdch_acm_write(acmh_handle_t *acm, const void *buf, size_t length, uint32_t timeout, cdch_acm_write_cb cb)

   向 CDC-ACM 实例发起一次异步写入。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``buf``
        - ``const void *``
        - 待发送数据的缓冲区。
      * - ``length``
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

.. c:function:: void cdch_acm_set_dtr(acmh_handle_t *acm, bool dtr)

   设置 DTR 信号。该状态与 RTS 一起在下一次 ``cdch_acm_drv_task()`` 中通过 ``SET_CONTROL_LINE_STATE`` 请求发送。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``dtr``
        - ``bool``
        - 为 ``true`` 时置位 DTR，为 ``false`` 时清除。

.. c:function:: void cdch_acm_set_rts(acmh_handle_t *acm, bool rts)

   设置 RTS 信号。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``rts``
        - ``bool``
        - 为 ``true`` 时置位 RTS，为 ``false`` 时清除。

.. c:function:: void cdch_acm_set_break(acmh_handle_t *acm, bool brk)

   设置 break 状态。为 ``true`` 时发送持续时间为 ``0xFFFF`` 的 break，为 ``false`` 时发送持续时间为 0 的 break。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``brk``
        - ``bool``
        - break 状态。

.. c:function:: void cdch_acm_send_break(acmh_handle_t *acm, uint16_t duration)

   发送指定持续时间的 break 信号。该函数无返回值。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。
      * - ``duration``
        - ``uint16_t``
        - break 持续时间。

.. c:function:: bool cdch_acm_get_dtr(acmh_handle_t *acm)

   获取当前 DTR 状态。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - DTR 已置位。
      * - ``false``
        - DTR 未置位。

.. c:function:: bool cdch_acm_get_rts(acmh_handle_t *acm)

   获取当前 RTS 状态。

   .. list-table::

      * - 参数
        - 类型
        - 说明
      * - ``acm``
        - ``acmh_handle_t *``
        - CDC-ACM 实例句柄。

   .. list-table::

      * - 返回值
        - 说明
      * - ``true``
        - RTS 已置位。
      * - ``false``
        - RTS 未置位。
