/**
 * @file usbh_driver_core.c
 * @author Links (lhd@wch.cn)
 * @brief USB host driver core source file
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @define */
#define USB_LOG_TAG "USBH"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"

#ifdef USB_HOST_DRIVER_EN

bool usbh_drv_open(usbh_handle_t *h)
{
    return true;
}

bool usbh_drv_close(usbh_handle_t *h)
{
    return true;
}

#endif // USB_HOST_DRIVER_EN
