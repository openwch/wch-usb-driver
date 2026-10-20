芯片支持
============================

本协议栈支持以下沁恒（WCH）RISC-V 芯片，芯片与 USB 控制器的对应关系如下。

.. list-table::

   * - 系列
     - 芯片
     - USB 控制器（索引）
     - 数量
   * - ch32v4x7
     - ch32v407vet6
     - USBHS1（0）、USBHS2（1）
     - 2
   * - ch32v4x7
     - ch32v407weu6
     - USBHS1（0）、USBHS2（1）
     - 2
   * - ch32v4x7
     - ch32v467vet6
     - USBHS1（0）、USBHS2（1）
     - 2
   * - ch32v4x7
     - ch32v467weu6
     - USBHS1（0）、USBHS2（1）
     - 2
   * - ch32v4x7
     - ch32v407ret6
     - USBHS（0）
     - 1
   * - ch32v4x7
     - ch32v467ret6
     - USBHS（0）
     - 1
   * - ch32v30x
     - ch32v303cbt6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v303rbt6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v303rct6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v303rct7
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v303vct6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v305cct6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v305fbp6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v305gbu6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v305rbt6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v307rct6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v307vct6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v307wcu6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v317vct6
     - USBFS（0）
     - 1
   * - ch32v30x
     - ch32v317wcu6
     - USBFS（0）
     - 1
   * - ch32v205
     - ch32v205cct6
     - USBFS（0）、USBHS（1）
     - 2
   * - ch32v205
     - ch32v205rct6
     - USBFS（0）、USBHS（1）
     - 2
   * - ch32v205
     - ch32v205vct6
     - USBFS（0）、USBHS（1）
     - 2
   * - ch32v205
     - ch32v203cct6
     - USBFS（0）
     - 1

USB 控制器
--------------------

.. list-table::

   * - IP
     - 移植层
     - 说明
   * - USBFS
     - ``port/usbfs/``
     - 设备与主机控制器
   * - USBHS
     - ``port/usbhs/``
     - 设备与主机控制器，兼容全速
