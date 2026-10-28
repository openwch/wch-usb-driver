/********************************** (C) COPYRIGHT *******************************
* File Name          : usb_driver.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB driver header file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef USB_DRIVER_H
#define USB_DRIVER_H

/* @include */
/* Standard library includes */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

/* USB driver includes */
#include "usb_config.h"
#include "usb_define.h"

/* USB core driver includes */
#include "device/usbd_driver_public.h"
#include "host/usbh_driver_public.h"

/* USB class driver includes */
#include "class/hub/hub.h"
#include "class/hid/hid.h"
#include "class/cdc/cdc.h"
#include "class/uac/uac.h"

/* USB device class driver includes */
#include "class/hid/hidd.h"
#include "class/cdc/cdcd_acm.h"
#include "class/cdc/cdcd_ecm.h"
#include "class/uac/uacd.h"

/* USB host class driver includes */
#include "class/hub/hubh.h"
#include "class/hid/hidh.h"
#include "class/cdc/cdch_acm.h"
#include "class/cdc/cdch_ecm.h"
#include "class/uac/uach.h"

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
