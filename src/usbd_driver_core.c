/********************************** (C) COPYRIGHT *******************************
* File Name          : usbd_driver_core.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB device driver core source file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "USBD"

/* @include */
#include "usb_driver.h"
#include "device/usbd_driver_private.h"

#ifdef USB_DEVICE_DRIVER_EN

static bool endp_stand_request_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    usb_endp_t endp = USB_U16_LOW(setup->wIndex);
    switch (setup->bRequest)
    {
    case USB_REQ_GET_STATUS:
        if (!USB_GET_REQ_DIR(setup->bmRequestType)) return false;
        h->stand_req_buf = h->endp_is_stalled(h, endp) ? 0x0001 : 0x0000;
        *buf = &h->stand_req_buf;
        *len = sizeof(uint16_t);
        return true;

    case USB_REQ_SET_FEATURE:
    case USB_REQ_CLEAR_FEATURE:
        if (USB_GET_REQ_DIR(setup->bmRequestType)) return false;
        if (setup->wValue == USB_FEATURE_EDPT_HALT)
        {
            return true;
        }
    }
    return false;
}

static void endp_stand_request_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    usb_endp_t endp = USB_U16_LOW(setup->wIndex);
    switch (setup->bRequest)
    {
    case USB_REQ_SET_FEATURE:
        h->endp_stall(h, endp, true);
        USB_LOGI("Handle %p: Set endpoint 0x%02X stall", h, endp);
        break;

    case USB_REQ_CLEAR_FEATURE:
        h->endp_stall(h, endp, false);
        USB_LOGI("Handle %p: Clear endpoint 0x%02X stall", h, endp);
        break;
    }
}

static void setup_event_handle(usbd_handle_t *h)
{
    bool rst = false;
    void *buf = NULL;
    size_t len = 0;
    usb_setup_t *setup = &h->setup;

    /* Get link speed */
    if (h->link_speed == USB_SPEED_UNKNOWN)
    {
        h->link_speed = h->get_link_speed(h);
    }

    usb_req_rcpt_t recipient = USB_GET_REQ_RCPT(setup->bmRequestType);

    switch (recipient)
    {
    case USB_REQ_RCPT_DEVICE:
        for (size_t i = 0; i < USB_ARRAY_SIZE(h->dev_req_cbs); i++)
        {
            usbd_dev_req_cb_t *dev_req = &h->dev_req_cbs[i];
            if (dev_req->bmRequestType == setup->bmRequestType && dev_req->bRequest == setup->bRequest)
            {
                h->ctrl_handle = h;
                h->ctrl_cbs = &dev_req->cbs;
                rst = dev_req->cbs.setup ? dev_req->cbs.setup(h, setup, &buf, &len) : true;
                break;
            }
        }
        break;

    case USB_REQ_RCPT_INTERFACE:
    {
        uint8_t itf_num = USB_U16_LOW(setup->wIndex);
        if (itf_num < USB_ARRAY_SIZE(h->itf_req_cbs))
        {
            usbd_itf_req_cb_t *itf_req = &h->itf_req_cbs[itf_num];
            if (itf_req->cbs.setup || itf_req->cbs.data || itf_req->cbs.status)
            {
                h->ctrl_handle = itf_req->class_handle;
                h->ctrl_cbs = &itf_req->cbs;
                rst = itf_req->cbs.setup ? itf_req->cbs.setup(itf_req->class_handle, setup, &buf, &len) : true;
            }
        }
        break;
    }

    case USB_REQ_RCPT_ENDPOINT:
        if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_STANDARD)
        {
            static const usbd_ctrl_xfer_cbs_t endp_stand_cbs = {
                .setup = endp_stand_request_setup,
                .data = NULL,
                .status = endp_stand_request_status,
            };

            h->ctrl_handle = h;
            h->ctrl_cbs = &endp_stand_cbs;
            rst = h->ctrl_cbs->setup ? h->ctrl_cbs->setup(h, setup, &buf, &len) : true;
        }
        else
        {
            usb_endp_t endp = USB_U16_LOW(setup->wIndex);
            for (size_t i = 0; i < USB_ARRAY_SIZE(h->endp_req_cbs); i++)
            {
                usbd_endp_req_cb_t *endp_req = &h->endp_req_cbs[i];
                if (!endp_req->endp || endp_req->endp != endp) continue;
                h->ctrl_handle = endp_req->class_handle;
                h->ctrl_cbs = &endp_req->cbs;
                rst = endp_req->cbs.setup ? endp_req->cbs.setup(endp_req->class_handle, setup, &buf, &len) : true;
                break;
            }
        }
        break;
    }

    if (rst)
    {
        USB_LOGI("Handle %p: Received setup: %02x %02x %04x %04x %04x", h, setup->bmRequestType, setup->bRequest,
                 setup->wValue, setup->wIndex, setup->wLength);
        h->ctrl_xfer_zlp = false;
        h->ctrl_xfer_buf = buf;
        h->ctrl_xfer_len = 0;
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
        USB_LOGW("Handle %p: Failed to handle setup: %02x %02x %04x %04x %04x", h, setup->bmRequestType,
                 setup->bRequest, setup->wValue, setup->wIndex, setup->wLength);
        h->endp_stall(h, 0x80, true);
        h->endp_stall(h, 0x00, true);
    }
}

static void xfer_event_handle(usbd_handle_t *h, usbd_port_event_ctx_t *ctx)
{
    usb_setup_t *setup = &h->setup;
    uint8_t endp_dir = USB_ENDP_DIR(ctx->xfer.endp);
    uint8_t endp_num = USB_ENDP_NUM(ctx->xfer.endp);

    /* Control transfer event handling */
    if (endp_num == 0)
    {
        /* Control transfer data stage */
        if ((setup->bmRequestType & 0x80) == (ctx->xfer.endp & 0x80))
        {
            /* Send zero-length packet to acknowledge the data stage */
            if (ctx->xfer.len && ctx->xfer.len < setup->wLength && ctx->xfer.len % h->ep0_mps == 0 &&
                setup->bmRequestType & 0x80 && !h->ctrl_xfer_zlp)
            {
                h->ctrl_xfer_zlp = true;
                h->ctrl_xfer_len = ctx->xfer.len;
                h->endp_transfer(h, 0x80, NULL, 0);
                return;
            }

            if (!h->ctrl_xfer_zlp)
            {
                h->ctrl_xfer_len = ctx->xfer.len;
            }

            bool rst = true;
            if (h->ctrl_cbs && h->ctrl_cbs->data)
            {
                rst = h->ctrl_cbs->data(h->ctrl_handle, setup, h->ctrl_xfer_buf, h->ctrl_xfer_len);
            }

            /* Handle the status stage based on the result of the data stage */
            if (rst)
            {
                h->endp_transfer(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x00 : 0x80, NULL, 0);
            }
            else
            {
                h->endp_stall(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x00 : 0x80, true);
            }
        }
        /* Control transfer status stage */
        else
        {
            if (h->ctrl_cbs && h->ctrl_cbs->status)
            {
                h->ctrl_cbs->status(h->ctrl_handle, setup, h->ctrl_xfer_buf, h->ctrl_xfer_len);
            }
        }
    }
    /* Isochronous/interrupt/bulk transfer event handling */
    else
    {
        usbd_data_xfer_cbs_t *xfer_cb = &h->xfer_cb_ctxs[endp_dir ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1];
        if (xfer_cb->cb)
        {
            xfer_cb->cb((void *)xfer_cb->class_handle, ctx->xfer.endp, ctx->xfer.buf, ctx->xfer.len);
        }
    }
}

static bool get_config_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    memset(&h->stand_req_buf, 0, sizeof(h->stand_req_buf));
    memcpy(&h->stand_req_buf, &h->config_num, sizeof(h->config_num));
    *buf = &h->stand_req_buf;
    *len = sizeof(h->config_num);
    return true;
}

static bool get_status_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    memset(&h->stand_req_buf, 0, sizeof(h->stand_req_buf));
    size_t size = 0;
    const void *desc = NULL;
    desc = h->get_desc_cb(h, USB_DESC_CONFIGURATION, h->config_num, h->link_speed, &size);
    if (desc && size >= sizeof(usb_desc_config_t))
    {
        usb_desc_config_t *config_desc = (usb_desc_config_t *)desc;
        h->stand_req_buf = (config_desc->bmAttributes & USB_SELF_POWERED_MASK ? 0x0001 : 0x0000) |
                           (h->remote_wakeup ? 0x0002 : 0x0000);
        *buf = &h->stand_req_buf;
        *len = sizeof(uint16_t);
        return true;
    }
    return false;
}

static bool set_clear_feature_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    return setup->wValue == USB_FEATURE_REMOTE_WAKEUP || setup->wValue == USB_FEATURE_TEST_MODE;
}

static bool get_descriptor_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    const void *desc = NULL;
    size_t desc_len = 0;
    desc = h->get_desc_cb(h, USB_U16_HIGH(setup->wValue), USB_U16_LOW(setup->wValue), h->link_speed, &desc_len);
    if (desc)
    {
        *buf = (void *)desc;
        *len = desc_len;
        return true;
    }
    return false;
}

static void set_address_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    uint8_t addr = USB_U16_LOW(setup->wValue) & 0x7F;
    USB_LOGI("Handle %p: Set bus address: %d", h, addr);
    h->set_address(h, addr);
}

static void set_config_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    uint8_t config_num = USB_U16_LOW(setup->wValue);
    USB_LOGI("Handle %p: Set configuration: %d", h, config_num);
    h->config_num = config_num;
    if (h->event_cbs[USBD_EVENT_ENUM_COMPLETED])
    {
        usbd_event_ctx_t event_ctx;
        event_ctx.e = USBD_EVENT_ENUM_COMPLETED;
        event_ctx.enum_completed.config_num = h->config_num;
        event_ctx.enum_completed.link_speed = h->link_speed;
        h->event_cbs[USBD_EVENT_ENUM_COMPLETED](h, &event_ctx);
    }
}

static void set_feature_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    if (setup->wValue == USB_FEATURE_REMOTE_WAKEUP)
    {
        USB_LOGI("Handle %p: Enable remote wakeup", h);
        h->remote_wakeup = true;
    }
    else if (setup->wValue == USB_FEATURE_TEST_MODE)
    {
        usb_test_select_t test_selector = USB_U16_HIGH(setup->wIndex);
        USB_LOGI("Handle %p: Enter test mode: %d", h, test_selector);
        h->test_mode_ctrl(h, test_selector);
    }
}

static void clear_feature_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    if (setup->wValue == USB_FEATURE_REMOTE_WAKEUP)
    {
        USB_LOGI("Handle %p: Disable remote wakeup", h);
        h->remote_wakeup = false;
    }
}

bool usbd_drv_open(usbd_handle_t *h, usb_speed_t speed, bool sof_en, usbd_get_desc_cb get_desc_cb)
{
    if (!h || !get_desc_cb) return false;

    h->get_desc_cb = get_desc_cb;

    size_t size = 0;
    usb_desc_device_t *desc = (usb_desc_device_t *)h->get_desc_cb(h, USB_DESC_DEVICE, 0, USB_SPEED_UNKNOWN, &size);

    /* Validate the endpoint 0 size */
    if (!desc || desc->bMaxPacketSize0 == 0 || desc->bMaxPacketSize0 > 64) return false;
    h->ep0_mps = desc->bMaxPacketSize0;

    /* Register standard USB request callbacks */
    usbd_ctrl_xfer_cbs_t cbs;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = get_config_setup;
    if (!usbd_register_dev_req_cb(h, 0x80, USB_REQ_GET_CONFIGURATION, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = get_status_setup;
    if (!usbd_register_dev_req_cb(h, 0x80, USB_REQ_GET_STATUS, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = set_clear_feature_setup;
    cbs.status = set_feature_status;
    if (!usbd_register_dev_req_cb(h, 0x00, USB_REQ_SET_FEATURE, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = set_clear_feature_setup;
    cbs.status = clear_feature_status;
    if (!usbd_register_dev_req_cb(h, 0x00, USB_REQ_CLEAR_FEATURE, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = get_descriptor_setup;
    if (!usbd_register_dev_req_cb(h, 0x80, USB_REQ_GET_DESCRIPTOR, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.status = set_address_status;
    if (!usbd_register_dev_req_cb(h, 0x00, USB_REQ_SET_ADDRESS, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.status = set_config_status;
    if (!usbd_register_dev_req_cb(h, 0x00, USB_REQ_SET_CONFIGURATION, &cbs)) return false;

    USB_LOGI("Handle %p: Successfully opened USB device driver", h);
    return h->open(h, speed, sof_en);
}

bool usbd_drv_close(usbd_handle_t *h)
{
    if (!h) return false;
    return h->close(h);
}

bool usbd_drv_resume(usbd_handle_t *h)
{
    if (!h || !h->remote_wakeup) return false;
    return h->resume(h);
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
        USB_LOGI("Handle %p: Bus reset", h);
        h->remote_wakeup = false;
        h->link_speed = USB_SPEED_UNKNOWN;
        h->config_num = 0;
        h->ctrl_handle = NULL;
        h->ctrl_xfer_zlp = false;
        h->ctrl_cbs = NULL;
        memset(h->itf_req_cbs, 0, sizeof(h->itf_req_cbs));
        memset(h->xfer_cb_ctxs, 0, sizeof(h->xfer_cb_ctxs));
        memset(h->endp_req_cbs, 0, sizeof(h->endp_req_cbs));
        h->set_address(h, 0);
        h->endp_open(h, 0x80, USB_ENDP_TYPE_CTRL, h->ep0_mps);
        h->endp_open(h, 0x00, USB_ENDP_TYPE_CTRL, h->ep0_mps);
        if (h->event_cbs[USBD_EVENT_RESET])
        {
            usbd_event_ctx_t event_ctx;
            event_ctx.e = USBD_EVENT_RESET;
            h->event_cbs[USBD_EVENT_RESET](h, &event_ctx);
        }
        break;

    case USBD_PORT_EVENT_SUSPEND:
        USB_LOGI("Handle %p: Bus suspend", h);
        if (h->event_cbs[USBD_EVENT_SUSPEND])
        {
            usbd_event_ctx_t event_ctx;
            event_ctx.e = USBD_EVENT_SUSPEND;
            h->event_cbs[USBD_EVENT_SUSPEND](h, &event_ctx);
        }
        break;
    }
}

bool usbd_register_dev_req_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest, usbd_ctrl_xfer_cbs_t *cbs)
{
    if (!h || USB_GET_REQ_RCPT(bmRequestType) != USB_REQ_RCPT_DEVICE) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->dev_req_cbs); i++)
    {
        usbd_dev_req_cb_t *dev_req = &h->dev_req_cbs[i];
        if ((dev_req->bmRequestType == 0 && dev_req->bRequest == 0) ||
            (dev_req->bmRequestType == bmRequestType && dev_req->bRequest == bRequest))
        {
            dev_req->bmRequestType = bmRequestType;
            dev_req->bRequest = bRequest;
            dev_req->cbs.setup = cbs->setup;
            dev_req->cbs.data = cbs->data;
            dev_req->cbs.status = cbs->status;
            return true;
        }
    }
    return false;
}

bool usbd_register_itf_req_cb(usbd_handle_t *h, void *class_handle, uint8_t itf_num, usbd_ctrl_xfer_cbs_t *cbs)
{
    if (!h || itf_num >= USB_ARRAY_SIZE(h->itf_req_cbs)) return false;
    usbd_itf_req_cb_t *itf_req = &h->itf_req_cbs[itf_num];
    itf_req->class_handle = class_handle;
    itf_req->cbs.setup = cbs->setup;
    itf_req->cbs.data = cbs->data;
    itf_req->cbs.status = cbs->status;
    return true;
}

bool usbd_register_endp_req_cb(usbd_handle_t *h, void *class_handle, usb_endp_t endp, usbd_ctrl_xfer_cbs_t *cbs)
{
    if (!h || !USB_ENDP_NUM(endp) || USB_ENDP_NUM(endp) >= USB_MAX_ENDP_NUM) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->endp_req_cbs); i++)
    {
        usbd_endp_req_cb_t *endp_req = &h->endp_req_cbs[i];
        if (endp_req->endp == 0 || endp_req->endp == endp)
        {
            endp_req->endp = endp;
            endp_req->class_handle = class_handle;
            endp_req->cbs.setup = cbs->setup;
            endp_req->cbs.data = cbs->data;
            endp_req->cbs.status = cbs->status;
            return true;
        }
    }
    return false;
}

bool usbd_unregister_dev_req_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest)
{
    if (!h) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->dev_req_cbs); i++)
    {
        usbd_dev_req_cb_t *dev_req = &h->dev_req_cbs[i];
        if (dev_req->bmRequestType == bmRequestType && dev_req->bRequest == bRequest)
        {
            dev_req->bmRequestType = 0;
            dev_req->bRequest = 0;
            dev_req->cbs.setup = NULL;
            dev_req->cbs.data = NULL;
            dev_req->cbs.status = NULL;
            return true;
        }
    }
    return false;
}

bool usbd_unregister_itf_req_cb(usbd_handle_t *h, uint8_t interface_num)
{
    if (!h || interface_num >= USB_ARRAY_SIZE(h->itf_req_cbs)) return false;
    usbd_itf_req_cb_t *itf_req = &h->itf_req_cbs[interface_num];
    itf_req->class_handle = NULL;
    itf_req->cbs.setup = NULL;
    itf_req->cbs.data = NULL;
    itf_req->cbs.status = NULL;
    return true;
}

bool usbd_unregister_endp_req_cb(usbd_handle_t *h, usb_endp_t endp)
{
    if (!h || !USB_ENDP_NUM(endp) || USB_ENDP_NUM(endp) >= USB_MAX_ENDP_NUM) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->endp_req_cbs); i++)
    {
        usbd_endp_req_cb_t *endp_req = &h->endp_req_cbs[i];
        if (endp_req->endp == endp)
        {
            endp_req->endp = 0;
            endp_req->class_handle = NULL;
            endp_req->cbs.setup = NULL;
            endp_req->cbs.data = NULL;
            endp_req->cbs.status = NULL;
            return true;
        }
    }
    return false;
}

bool usbd_endp_open(usbd_handle_t *h, void *class_handle, const usb_desc_endpoint_t *ep_desc, usbd_data_xfer_cb cb)
{
    if (!h || !ep_desc) return false;
    usb_endp_t endp = ep_desc->bEndpointAddress;
    uint8_t endp_num = USB_ENDP_NUM(endp);
    if (endp_num == 0) return false;
    usb_endp_type_t endp_type = USB_ENDP_GET_TYPE(ep_desc->bmAttributes);
    uint16_t endp_mps = USB_ENDP_GET_MPS(ep_desc->wMaxPacketSize);

    usbd_data_xfer_cbs_t *xfer_cb = &h->xfer_cb_ctxs[USB_ENDP_DIR(endp) ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1];
    xfer_cb->class_handle = class_handle;
    xfer_cb->cb = cb;
    return h->endp_open(h, endp, endp_type, endp_mps);
}

bool usbd_endp_close(usbd_handle_t *h, usb_endp_t endp)
{
    if (!h) return false;
    uint8_t endp_num = USB_ENDP_NUM(endp);
    if (endp_num == 0) return false;
    if (h->endp_close(h, endp))
    {
        usbd_data_xfer_cbs_t *xfer_cb = &h->xfer_cb_ctxs[USB_ENDP_DIR(endp) ? USB_DIR_IN : USB_DIR_OUT][endp_num - 1];
        xfer_cb->cb = NULL;
        xfer_cb->class_handle = NULL;
        return true;
    }
    return false;
}

bool usbd_endp_is_busy(usbd_handle_t *h, usb_endp_t endp)
{
    return h->endp_is_busy(h, endp);
}

bool usbd_endp_read(usbd_handle_t *h, usb_endp_t endp, void *buf, size_t len)
{
    return h->endp_transfer(h, USB_ENDP_NUM(endp), buf, len);
}

bool usbd_endp_write(usbd_handle_t *h, usb_endp_t endp, const void *buf, size_t len)
{
    return h->endp_transfer(h, 0x80 | endp, (void *)buf, len);
}

#endif // USB_DEVICE_DRIVER_EN
