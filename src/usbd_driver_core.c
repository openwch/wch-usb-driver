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

static bool get_config_setup(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len);
static bool get_status_setup(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len);
static bool set_address_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len);
static bool set_config_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len);
static bool set_feature_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len);
static bool clear_feature_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len);

bool usbd_drv_open(usbd_handle_t *h, usb_speed_t speed, bool sof_en)
{
    if (!h) return false;
    h->ep0_mps = speed == USB_SPEED_LOW ? 8 : 64;

    /* Register standard USB request callbacks */
    usbd_ctrl_xfer_cb cbs[3];

    memset(cbs, 0, sizeof(cbs));
    cbs[USB_CTRL_STAGE_SETUP] = get_config_setup;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_GET_CONFIGURATION, cbs)) return false;

    memset(cbs, 0, sizeof(cbs));
    cbs[USB_CTRL_STAGE_SETUP] = get_status_setup;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_GET_STATUS, cbs)) return false;

    memset(cbs, 0, sizeof(cbs));
    cbs[USB_CTRL_STAGE_STATUS] = set_address_status;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_SET_ADDRESS, cbs)) return false;

    memset(cbs, 0, sizeof(cbs));
    cbs[USB_CTRL_STAGE_STATUS] = set_config_status;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_SET_CONFIGURATION, cbs)) return false;

    memset(cbs, 0, sizeof(cbs));
    cbs[USB_CTRL_STAGE_STATUS] = set_feature_status;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_SET_FEATURE, cbs)) return false;

    memset(cbs, 0, sizeof(cbs));
    cbs[USB_CTRL_STAGE_STATUS] = clear_feature_status;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_CLEAR_FEATURE, cbs)) return false;

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
    else if (endp_ctx->cb)
    {
        if (endp_ctx->cb(h, ctx->xfer.endp, endp_ctx->xfer_buf, endp_ctx->xfer_ofs))
        {
            h->endp_transfer(h, ctx->xfer.endp, endp_ctx->xfer_buf, endp_ctx->xfer_len);
        }
    }
}

static bool get_config_setup(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len)
{
    memset(&h->stand_req_buf, 0, sizeof(h->stand_req_buf));
    memcpy(&h->stand_req_buf, &h->config_num, sizeof(h->config_num));
    *buf = &h->stand_req_buf;
    *len = sizeof(h->config_num);
    return true;
}

static bool get_status_setup(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len)
{
    switch (USB_GET_REQ_RCPT(setup->bmRequestType))
    {
    case USB_RCPT_DEVICE:
        memset(&h->stand_req_buf, 0, sizeof(h->stand_req_buf));
        h->stand_req_buf = (h->self_powered ? 0x0001 : 0x0000) | (h->remote_wakeup ? 0x0002 : 0x0000);
        *buf = &h->stand_req_buf;
        *len = sizeof(uint16_t);
        return true;

    case USB_RCPT_ENDPOINT:
        usb_endp_t endp = setup->wIndex & 0xFF;
        memset(&h->stand_req_buf, 0, sizeof(h->stand_req_buf));
        h->stand_req_buf = h->endp_is_stalled(h, endp) ? 0x0001 : 0x0000;
        *buf = &h->stand_req_buf;
        *len = sizeof(uint16_t);
        return true;
    }
    return false;
}

static bool set_address_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len)
{
    USB_LOGI("Setting USB address to %d", setup->wValue & 0x7F);
    h->set_address(h, setup->wValue & 0x7F);
    return true;
}

static bool set_config_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len)
{
    USB_LOGI("Setting USB configuration to %d", setup->wValue & 0xFF);
    h->config_num = setup->wValue & 0xFF;
    if (h->event_cbs[USBD_EVENT_ENUM_COMPLETED])
    {
        usbd_event_ctx_t event_ctx;
        event_ctx.e = USBD_EVENT_ENUM_COMPLETED;
        event_ctx.enum_completed.config_num = h->config_num;
        h->event_cbs[USBD_EVENT_ENUM_COMPLETED](h, &event_ctx);
    }
    return true;
}

static bool set_feature_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len)
{
    switch (USB_GET_REQ_RCPT(setup->bmRequestType))
    {
    case USB_RCPT_DEVICE:
        if (setup->wValue == USB_FEATURE_REMOTE_WAKEUP)
        {
            h->remote_wakeup = true;
            USB_LOGI("Enabled remote wakeup");
        }
        else if (setup->wValue == USB_FEATURE_TEST_MODE)
        {
            usb_test_select_t test_selector = (setup->wIndex >> 8) & 0xFF;
            h->test_mode_ctrl(h, test_selector);
            USB_LOGI("Entered test mode: %d", test_selector);
        }
        break;

    case USB_RCPT_ENDPOINT:
        if (setup->wValue == USB_FEATURE_EDPT_HALT)
        {
            usb_endp_t endp = setup->wIndex & 0xFF;
            h->endp_stall(h, endp, true);
            USB_LOGI("Stalled endpoint 0x%02X", endp);
        }
        break;
    }
    return true;
}

static bool clear_feature_status(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len)
{
    switch (USB_GET_REQ_RCPT(setup->bmRequestType))
    {
    case USB_RCPT_DEVICE:
        if (setup->wValue == USB_FEATURE_REMOTE_WAKEUP)
        {
            h->remote_wakeup = false;
            USB_LOGI("Disabled remote wakeup");
        }
        break;

    case USB_RCPT_ENDPOINT:
        if (setup->wValue == USB_FEATURE_EDPT_HALT)
        {
            usb_endp_t endp = setup->wIndex & 0xFF;
            h->endp_stall(h, endp, false);
            USB_LOGI("Cleared stall on endpoint 0x%02X", endp);
        }
        break;
    }
    return true;
}
