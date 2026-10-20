快速开始
============================

本节以 ``examples/device/hid_km`` 为例，介绍如何编译并运行第一个示例。

环境准备
--------------------

编译需要 Make 和 GCC 交叉编译工具链，二者均由 MounRiver Studio 2（MRS2）提供。更多信息请访问 `MounRiver Studio 2 官方网站 <https://www.mounriver.com>`_。将以下目录（按实际安装位置调整）加入系统环境变量 ``Path``：

.. code-block:: text

   C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin
   C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC15\bin

重新打开终端，验证环境是否配置成功：

.. code-block:: bash

   make -v
   riscv32-wch-elf-gcc -v

能正常输出版本信息即可。

获取源码
--------------------

.. code-block:: bash

   git clone https://github.com/openwch/wch-usb-driver.git
   cd wch-usb-driver

编译示例
--------------------

.. code-block:: bash

   cd examples/device/hid_km
   make all -j8 CHIP=ch32v205rct6

- ``CHIP`` 必须是该示例支持的芯片，完整列表见示例 ``makefile`` 中的 ``SUPPORTED_CHIPS``，也可参考 :doc:`supported_chips`。
- 编译产物位于 ``build/<chip>/output/``，包含 ``.elf``、``.bin``、``.hex``、``.lst`` 和 ``.map``。
- 执行 ``make clear`` 可清理产物。

烧录与验证
--------------------

使用 WCH-LinkUtility 或 WCHISPStudio，选择对应芯片，烧录 ``.hex`` 或 ``.bin`` 文件。

HID-KM 示例运行后，将 USB 接口连接到电脑，系统会识别出一个键盘和鼠标设备。
