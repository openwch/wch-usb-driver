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

void usbh_drv_task(usbh_handle_t *h)
{
    if (h->root_port_change)
    {
        h->root_port_change = false;
    }
}

bool usbh_drv_open(usbh_handle_t *h)
{
    if (h == NULL) return false;
    return h->open(h);
}

bool usbh_drv_close(usbh_handle_t *h)
{
    if (h == NULL) return false;
    return h->close(h);
}

#endif // USB_HOST_DRIVER_EN
