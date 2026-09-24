/**
 * @file usbd_driver_core.c
 * @author Links (lhd@wch.cn)
 * @brief USB device driver core source file
 *
 * @copyright Copyright (c) 2026
 *
 */

#define USB_LOG_TAG "USBD"

/* @include */
#include "usb_driver.h"
#include "device/usbd_driver_private.h"

/* @function declaration */
static void setup_event_handle(usbd_handle_t *h);
static void xfer_event_handle(usbd_handle_t *h, usbd_port_event_ctx_t *ctx);

static bool get_desc_setup(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len);
static bool set_address_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len);

bool usbd_drv_open(usbd_handle_t *h, usb_speed_t speed, bool sof_en)
{
    if (!h) return false;
    h->ep0_mps = speed == USB_SPEED_LOW ? 8 : 64;

    usbd_ctrl_xfer_cb cbs[3];

    /* Register the SET_ADDRESS request callback */
    cbs[USB_CTRL_STAGE_SETUP] = NULL;
    cbs[USB_CTRL_STAGE_DATA] = NULL;
    cbs[USB_CTRL_STAGE_STATUS] = set_address_status;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_SET_ADDRESS, cbs)) return false;

    cbs[USB_CTRL_STAGE_SETUP] = get_desc_setup;
    cbs[USB_CTRL_STAGE_DATA] = NULL;
    cbs[USB_CTRL_STAGE_STATUS] = NULL;
    if (!usbd_register_request_cb(h, 0x80, USB_REQ_GET_DESCRIPTOR, cbs)) return false;

    return h->open(h, speed, sof_en);
}

bool usbd_register_event_callback(usbd_handle_t *h, usbd_event_t event, usbd_event_cb cb)
{
    if (!h || event >= USBD_EVENT_COUNT || !cb) return false;
    h->event_cbs[event] = cb;
    return true;
}

bool usbd_unregister_event_callback(usbd_handle_t *h, usbd_event_t event)
{
    if (!h || event >= USBD_EVENT_COUNT) return false;
    h->event_cbs[event] = NULL;
    return true;
}

void usbd_event_handle(usbd_handle_t *h, usbd_port_event_ctx_t *ctx)
{
    switch (ctx->e)
    {
    case USBD_PORT_EVENT_XFER:
        xfer_event_handle(h, ctx);
        break;

    case USBD_PORT_EVENT_SOF:
        if (h->event_cbs[USBD_EVENT_SOF])
        {
            usbd_event_ctx_t event_ctx;
            event_ctx.e = USBD_EVENT_SOF;
            event_ctx.sof.frame_num = ctx->sof.frame_num;
            event_ctx.sof.mframe_num = ctx->sof.mframe_num;
            h->event_cbs[USBD_EVENT_SOF](h, &event_ctx);
        }
        break;

    case USBD_PORT_EVENT_SETUP:
        setup_event_handle(h);
        break;

    case USBD_PORT_EVENT_RESET:
        h->set_address(h, 0);
        h->endp_open(h, 0x00, USB_ENDP_TYPE_CTRL, h->ep0_mps);
        h->endp_open(h, 0x80, USB_ENDP_TYPE_CTRL, h->ep0_mps);
        h->endp_transfer(h, 0x80, &h->setup, sizeof(usb_setup_t));
        if (h->event_cbs[USBD_EVENT_RESET])
        {
            usbd_event_ctx_t event_ctx;
            event_ctx.e = USBD_EVENT_RESET;
            h->event_cbs[USBD_EVENT_RESET](h, &event_ctx);
        }
        break;

    case USBD_PORT_EVENT_SUSPEND:
        if (h->event_cbs[USBD_EVENT_SUSPEND])
        {
            usbd_event_ctx_t event_ctx;
            event_ctx.e = USBD_EVENT_SUSPEND;
            h->event_cbs[USBD_EVENT_SUSPEND](h, &event_ctx);
        }
        break;
    }
}

bool usbd_register_request_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest, usbd_ctrl_xfer_cb cb[3])
{
    if (!h) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->request_cbs); i++)
    {
        usbd_request_cb_t *req_cb = &h->request_cbs[i];
        if ((req_cb->bmRequestType == 0 && req_cb->bRequest == 0) ||
            (req_cb->bmRequestType == bmRequestType && req_cb->bRequest == bRequest))
        {
            req_cb->bmRequestType = bmRequestType;
            req_cb->bRequest = bRequest;
            req_cb->cb[USB_CTRL_STAGE_SETUP] = cb[USB_CTRL_STAGE_SETUP];
            req_cb->cb[USB_CTRL_STAGE_DATA] = cb[USB_CTRL_STAGE_DATA];
            req_cb->cb[USB_CTRL_STAGE_STATUS] = cb[USB_CTRL_STAGE_STATUS];
            return true;
        }
    }
    return false;
}

bool usbd_register_interface_cb(usbd_handle_t *h, uint8_t interface_num, usbd_ctrl_xfer_cb cb[3])
{
    if (!h || interface_num >= USB_ARRAY_SIZE(h->interface_cbs)) return false;
    usbd_interface_cb_t *itf_cb = &h->interface_cbs[interface_num];
    itf_cb->cb[USB_CTRL_STAGE_SETUP] = cb[USB_CTRL_STAGE_SETUP];
    itf_cb->cb[USB_CTRL_STAGE_DATA] = cb[USB_CTRL_STAGE_DATA];
    itf_cb->cb[USB_CTRL_STAGE_STATUS] = cb[USB_CTRL_STAGE_STATUS];
    return true;
}

bool usbd_unregister_request_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest)
{
    if (!h) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->request_cbs); i++)
    {
        usbd_request_cb_t *req_cb = &h->request_cbs[i];
        if (req_cb->bmRequestType == bmRequestType && req_cb->bRequest == bRequest)
        {
            req_cb->bmRequestType = 0;
            req_cb->bRequest = 0;
            req_cb->cb[USB_CTRL_STAGE_SETUP] = NULL;
            req_cb->cb[USB_CTRL_STAGE_DATA] = NULL;
            req_cb->cb[USB_CTRL_STAGE_STATUS] = NULL;
            return true;
        }
    }
    return true;
}

bool usbd_unregister_interface_cb(usbd_handle_t *h, uint8_t interface_num)
{
    if (!h || interface_num >= USB_ARRAY_SIZE(h->interface_cbs)) return false;
    usbd_interface_cb_t *itf_cb = &h->interface_cbs[interface_num];
    itf_cb->cb[USB_CTRL_STAGE_SETUP] = NULL;
    itf_cb->cb[USB_CTRL_STAGE_DATA] = NULL;
    itf_cb->cb[USB_CTRL_STAGE_STATUS] = NULL;
    return true;
}

static void setup_event_handle(usbd_handle_t *h)
{
    bool rst = false;
    void *buf = NULL;
    size_t len = 0;
    usb_setup_t *setup = &h->setup;
    if (USB_GET_REQ_RCPT(setup->bmRequestType) == USB_RCPT_INTERFACE)
    {
        if (setup->wIndex < USB_ARRAY_SIZE(h->interface_cbs))
        {
            usbd_interface_cb_t *itf_cb = &h->interface_cbs[setup->wIndex];
            rst = itf_cb->cb[USB_CTRL_STAGE_SETUP] ? itf_cb->cb[USB_CTRL_STAGE_SETUP](h, setup, &buf, &len) : true;
            h->data_stage_cb = itf_cb->cb[USB_CTRL_STAGE_DATA];
            h->status_stage_cb = itf_cb->cb[USB_CTRL_STAGE_STATUS];
        }
    }
    else
    {
        for (size_t i = 0; i < USB_ARRAY_SIZE(h->request_cbs); i++)
        {
            usbd_request_cb_t *req_cb = &h->request_cbs[i];
            if (req_cb->bmRequestType == setup->bmRequestType && req_cb->bRequest == setup->bRequest)
            {
                rst = req_cb->cb[USB_CTRL_STAGE_SETUP] ? req_cb->cb[USB_CTRL_STAGE_SETUP](h, setup, &buf, &len) : true;
                h->data_stage_cb = req_cb->cb[USB_CTRL_STAGE_DATA];
                h->status_stage_cb = req_cb->cb[USB_CTRL_STAGE_STATUS];
                break;
            }
        }
    }

    if (rst)
    {
        USB_LOGI("Processed setup packet: %02x %02x %04x %04x %04x",
                 setup->bmRequestType,
                 setup->bRequest,
                 setup->wValue,
                 setup->wIndex,
                 setup->wLength);

        if (setup->wLength)
        {
            len = USB_MIN(setup->wLength, len);
            h->endp_transfer(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x80 : 0x00, buf, len);
        }
        else
        {
            h->endp_transfer(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x00 : 0x80, NULL, 0);
        }
    }
    else
    {
        USB_LOGE("Failed to handle setup packet: %02x %02x %04x %04x %04x",
                 setup->bmRequestType,
                 setup->bRequest,
                 setup->wValue,
                 setup->wIndex,
                 setup->wLength);

        h->endp_stall(h, 0x80, true);
        h->endp_stall(h, 0x00, true);
    }
}

static void xfer_event_handle(usbd_handle_t *h, usbd_port_event_ctx_t *ctx)
{
    usb_setup_t *setup = &h->setup;
    uint8_t dir = USB_ENDP_DIR(ctx->xfer.endp);
    uint8_t num = USB_ENDP_NUM(ctx->xfer.endp);
    usbd_endp_ctx_t *endp_ctx = &h->endp_ctxs[dir ? USB_DIR_IN : USB_DIR_OUT][num];

    if (num == 0)
    {
        // Control transfer data stage
        if ((setup->bmRequestType & 0x80) == (ctx->xfer.endp & 0x80))
        {
            bool rst = true;
            if (h->data_stage_cb)
            {
                rst = h->data_stage_cb(h, setup, &endp_ctx->xfer_buf, &endp_ctx->xfer_ofs);
            }

            // Send zero-length packet to acknowledge the data stage
            h->endp_transfer(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x80 : 0x00, NULL, 0);

            // Handle the status stage based on the result of the data stage
            if (rst)
            {
                h->endp_transfer(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x00 : 0x80, NULL, 0);
            }
            else
            {
                h->endp_stall(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x00 : 0x80, true);
            }
        }
        // Control transfer status stage
        else
        {
            h->endp_transfer(h, 0x80, &h->setup, sizeof(usb_setup_t));
            if (h->status_stage_cb)
            {
                h->status_stage_cb(h, setup, &endp_ctx->xfer_buf, &endp_ctx->xfer_ofs);
            }
        }
    }
    else
    {
    }
}

static bool get_desc_setup(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len)
{
    static usb_desc_device_t device_desc = {
        .bLength = sizeof(usb_desc_device_t),
        .bDescriptorType = USB_DESC_DEVICE,
        .bcdUSB = 0x0200,
        .bDeviceClass = 0x00,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize0 = 64,
        .idVendor = 0x1A86,
        .idProduct = 0xFE64,
        .bcdDevice = 0x0100,
        .iManufacturer = 1,
        .iProduct = 2,
        .iSerialNumber = 3,
        .bNumConfigurations = 1,
    };

    switch (setup->wValue >> 8)
    {
    case USB_DESC_DEVICE:
        *buf = &device_desc;
        *len = sizeof(device_desc);
        return true;

    default:
        return false;
    }
}

static bool set_address_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len)
{
    USB_LOGI("Setting USB address to 0x%02x", setup->wValue & 0x7F);
    h->set_address(h, setup->wValue & 0x7F);
    return true;
}
