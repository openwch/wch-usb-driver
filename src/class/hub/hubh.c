/**
 * @file hubh.c
 * @author Links (lhd@wch.cn)
 * @brief USB HUB host class driver
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @define */
#define USB_LOG_TAG "HUBH"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"

#ifdef USB_CLASS_HUBH_DRIVER_EN

/* @global */
static hubh_handle_t handle_pool[HUBH_HANDLE_POOL_SIZE];

/* @function declaration */
static bool hubh_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length);
static bool hubh_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num);

static const usbh_drv_info_t hubh_drv_info = {
    .vid = 0x0000,
    .pid = 0x0000,
    .dev_class = USB_CLASS_HUB,
    .dev_subclass = 0x00,
    .dev_protocol = 0x00,
    .itf_class = USB_CLASS_HUB,
    .itf_subclass = 0x00,
    .itf_protocol = 0x00,
    .match_bitmap = (USBH_DRV_MATCH_DEV_CLASS | USBH_DRV_MATCH_ITF_CLASS),
    .bind = hubh_drv_bind_cb,
    .unbind = hubh_drv_unbind_cb,
};

void hubh_drv_task(void)
{
}

bool hubh_drv_open(usbh_handle_t *h)
{
    if (h == NULL) return false;
    return usbh_register_driver(h, &hubh_drv_info);
}

bool hubh_drv_close(usbh_handle_t *h)
{
    if (h == NULL) return false;
    return usbh_unregister_driver(h, &hubh_drv_info);
}

static bool hubh_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length)
{
    return true;
}

static bool hubh_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num)
{
    return true;
}

#endif // USB_CLASS_HUBH_DRIVER_EN
