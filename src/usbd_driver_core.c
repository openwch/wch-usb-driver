/**
 * @file usbd_driver_core.c
 * @author Links (lhd@wch.cn)
 * @brief USB device driver core source file
 *
 * @copyright Copyright (c) 2026
 *
 */

/* #include */
#include "usb_driver.h"
#include "device/usbd_driver_private.h"

bool usbd_drv_open(usbd_handle_t *h, usb_speed_t speed, bool sof_en)
{
    if (!h) return false;

    h->ep0_mps = speed == USB_SPEED_LOW ? 8 : 64;

    return h->open(h, speed, sof_en);
}

void usbd_event_handle(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    switch (ctx->e)
    {
    case USBD_EVENT_XFER:
        break;

    case USBD_EVENT_SOF:
        break;

    case USBD_EVENT_SETUP:
        printf("Setup packet received %02x %02x %04x %04x %04x\n",
               ctx->setup.setup->bmRequestType,
               ctx->setup.setup->bRequest,
               ctx->setup.setup->wValue,
               ctx->setup.setup->wIndex,
               ctx->setup.setup->wLength);
        break;

    case USBD_EVENT_RESET:
        h->set_address(h, 0);
        h->endp_open(h, 0x00, USB_ENDP_TYPE_CTRL, h->ep0_mps);
        h->endp_open(h, 0x80, USB_ENDP_TYPE_CTRL, h->ep0_mps);
        break;

    case USBD_EVENT_SUSPEND:
        break;
    }
}
