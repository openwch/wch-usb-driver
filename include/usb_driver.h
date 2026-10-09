/**
 * @file usb_driver.h
 * @author Links (lhd@wch.cn)
 * @brief USB driver header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USB_DRIVER_H
#define USB_DRIVER_H

/* @include */
/* Standard library includes */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* USB driver includes */
#include "usb_config.h"
#include "usb_define.h"

/* USB core driver includes */
#include "device/usbd_driver_public.h"
#include "host/usbh_driver_public.h"

/* USB class driver includes */
#include "class/hid/hid.h"
#include "class/cdc/cdc.h"
#include "class/cdc/cdc_acm.h"
#include "class/hid/hidd.h"
#include "class/cdc/cdcd_acm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define USB_DRIVER_VERSION_STRING "v1.0.0"
#define USB_DRIVER_VERSION_NUMBER 0x0100

#ifdef __cplusplus
}
#endif

#endif // USB_DRIVER_H
