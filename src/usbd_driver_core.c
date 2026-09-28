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

    if (USB_GET_REQ_RCPT(setup->bmRequestType) == USB_REQ_RCPT_INTERFACE)
    {
        if (setup->wIndex < USB_ARRAY_SIZE(h->interface_cbs))
        {
            usbd_interface_cbs_t *itf_cbs = &h->interface_cbs[setup->wIndex];

            if (itf_cbs->cbs.setup || itf_cbs->cbs.data || itf_cbs->cbs.status)
            {
                h->ctrl_handle = itf_cbs->itf_handle;
                h->ctrl_cbs = &itf_cbs->cbs;
                rst = itf_cbs->cbs.setup ? itf_cbs->cbs.setup(itf_cbs->itf_handle, setup, &buf, &len) : true;
            }
        }
    }
    else
    {
        for (size_t i = 0; i < USB_ARRAY_SIZE(h->request_cbs); i++)
        {
            usbd_request_cbs_t *req_cbs = &h->request_cbs[i];
            if (req_cbs->bmRequestType == setup->bmRequestType && req_cbs->bRequest == setup->bRequest)
            {
                h->ctrl_handle = h;
                h->ctrl_cbs = &req_cbs->cbs;
                rst = req_cbs->cbs.setup ? req_cbs->cbs.setup(h, setup, &buf, &len) : true;
                break;
            }
        }
    }

    if (rst)
    {
        USB_LOGI("Processed setup packet: %02x %02x %04x %04x %04x", setup->bmRequestType, setup->bRequest,
                 setup->wValue, setup->wIndex, setup->wLength);

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
        USB_LOGE("Failed to handle setup packet: %02x %02x %04x %04x %04x", setup->bmRequestType, setup->bRequest,
                 setup->wValue, setup->wIndex, setup->wLength);

        h->endp_transfer(h, 0x00, &h->setup, sizeof(usb_setup_t));
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

    /* Control transfer event handling */
    if (num == 0)
    {
        /* Control transfer data stage */
        if ((setup->bmRequestType & 0x80) == (ctx->xfer.endp & 0x80))
        {
            bool rst = true;
            h->ctrl_xfer_buf = endp_ctx->xfer_buf;
            h->ctrl_xfer_len = endp_ctx->xfer_ofs;
            if (h->ctrl_cbs && h->ctrl_cbs->data)
            {
                rst = h->ctrl_cbs->data(h->ctrl_handle, setup, h->ctrl_xfer_buf, h->ctrl_xfer_len);
            }

            /* Send zero-length packet to acknowledge the data stage */
            h->endp_transfer(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x80 : 0x00, NULL, 0);

            /* Handle the status stage based on the result of the data stage */
            if (rst)
            {
                h->endp_transfer(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x00 : 0x80, NULL, 0);
            }
            else
            {
                h->endp_transfer(h, 0x00, &h->setup, sizeof(usb_setup_t));
                h->endp_stall(h, USB_GET_REQ_DIR(setup->bmRequestType) ? 0x00 : 0x80, true);
            }
        }
        /* Control transfer status stage */
        else
        {
            h->endp_transfer(h, 0x00, &h->setup, sizeof(usb_setup_t));
            if (h->ctrl_cbs && h->ctrl_cbs->status)
            {
                h->ctrl_cbs->status(h->ctrl_handle, setup, h->ctrl_xfer_buf, h->ctrl_xfer_len);
            }
        }
    }
    /* Isochronous/interrupt/bulk transfer event handling */
    else if (endp_ctx->cb)
    {
        if (endp_ctx->cb(h, ctx->xfer.endp, endp_ctx->xfer_buf, endp_ctx->xfer_ofs))
        {
            h->endp_transfer(h, ctx->xfer.endp, endp_ctx->xfer_buf, endp_ctx->xfer_len);
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
    switch (USB_GET_REQ_RCPT(setup->bmRequestType))
    {
    case USB_REQ_RCPT_DEVICE:
        memset(&h->stand_req_buf, 0, sizeof(h->stand_req_buf));
        size_t size = 0;
        usb_desc_config_t *desc = (usb_desc_config_t *)h->get_desc_cb(USB_DESC_CONFIGURATION, h->link_speed, &size);
        if (desc)
        {
            h->stand_req_buf = (desc->bmAttributes & USB_SELF_POWERED_MASK ? 0x0001 : 0x0000) |
                               (h->remote_wakeup ? 0x0002 : 0x0000);
            *buf = &h->stand_req_buf;
            *len = sizeof(uint16_t);
            return true;
        }
        break;

    case USB_REQ_RCPT_ENDPOINT:
        usb_endp_t endp = USB_U16_LOW(setup->wIndex);
        memset(&h->stand_req_buf, 0, sizeof(h->stand_req_buf));
        h->stand_req_buf = h->endp_is_stalled(h, endp) ? 0x0001 : 0x0000;
        *buf = &h->stand_req_buf;
        *len = sizeof(uint16_t);
        return true;
    }
    return false;
}

static bool set_clear_feature_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    switch (USB_GET_REQ_RCPT(setup->bmRequestType))
    {
    case USB_REQ_RCPT_DEVICE:
        if (setup->wValue == USB_FEATURE_REMOTE_WAKEUP || setup->wValue == USB_FEATURE_TEST_MODE)
        {
            return true;
        }
        break;

    case USB_REQ_RCPT_ENDPOINT:
        if (setup->wValue == USB_FEATURE_EDPT_HALT)
        {
            return true;
        }
        break;
    }
    return false;
}

static bool get_descriptor_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    uint8_t desc_type = USB_U16_HIGH(setup->wValue);
    const void *desc = NULL;
    size_t desc_len = 0;

    switch (desc_type)
    {
    case USB_DESC_DEVICE:
        desc = h->get_desc_cb(desc_type, 0, &desc_len);
        break;

    case USB_DESC_CONFIGURATION:
        desc = h->get_desc_cb(desc_type, h->link_speed, &desc_len);
        break;

    case USB_DESC_STRING:
        desc = h->get_desc_cb(desc_type, USB_U16_LOW(setup->wValue), &desc_len);
        break;

    case USB_DESC_DEVICE_QUALIFIER:
        desc = h->get_desc_cb(desc_type, 0, &desc_len);
        break;

    case USB_DESC_OTHER_SPEED_CONFIG:
        desc = h->get_desc_cb(desc_type, h->link_speed, &desc_len);
        break;

    case USB_DESC_BOS:
        desc = h->get_desc_cb(desc_type, 0, &desc_len);
        break;
    }

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
    USB_LOGI("Setting USB address to %d", addr);
    h->set_address(h, addr);
}

static void set_config_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    uint8_t config_num = USB_U16_LOW(setup->wValue);
    USB_LOGI("Setting USB configuration to %d", config_num);
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
    switch (USB_GET_REQ_RCPT(setup->bmRequestType))
    {
    case USB_REQ_RCPT_DEVICE:
        if (setup->wValue == USB_FEATURE_REMOTE_WAKEUP)
        {
            h->remote_wakeup = true;
            USB_LOGI("Enabled remote wakeup");
        }
        else if (setup->wValue == USB_FEATURE_TEST_MODE)
        {
            usb_test_select_t test_selector = USB_U16_HIGH(setup->wIndex);
            h->test_mode_ctrl(h, test_selector);
            USB_LOGI("Entered test mode: %d", test_selector);
        }
        break;

    case USB_REQ_RCPT_ENDPOINT:
        if (setup->wValue == USB_FEATURE_EDPT_HALT)
        {
            usb_endp_t endp = USB_U16_LOW(setup->wIndex);
            h->endp_stall(h, endp, true);
            USB_LOGI("Stalled endpoint 0x%02X", endp);
        }
        break;
    }
}

static void clear_feature_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    usbd_handle_t *h = (usbd_handle_t *)handle;
    switch (USB_GET_REQ_RCPT(setup->bmRequestType))
    {
    case USB_REQ_RCPT_DEVICE:
        if (setup->wValue == USB_FEATURE_REMOTE_WAKEUP)
        {
            h->remote_wakeup = false;
            USB_LOGI("Disabled remote wakeup");
        }
        break;

    case USB_REQ_RCPT_ENDPOINT:
        if (setup->wValue == USB_FEATURE_EDPT_HALT)
        {
            usb_endp_t endp = USB_U16_LOW(setup->wIndex);
            h->endp_stall(h, endp, false);
            USB_LOGI("Cleared stall on endpoint 0x%02X", endp);
        }
        break;
    }
}

bool usbd_drv_open(usbd_handle_t *h, usb_speed_t speed, bool sof_en, usbd_get_desc_cb get_desc_cb)
{
    if (!h || !get_desc_cb) return false;

    h->get_desc_cb = get_desc_cb;

    size_t size = 0;
    usb_desc_device_t *desc = (usb_desc_device_t *)h->get_desc_cb(USB_DESC_DEVICE, 0, &size);

    /* Validate the endpoint 0 size */
    if (!desc || desc->bMaxPacketSize0 == 0 || desc->bMaxPacketSize0 > 64) return false;
    h->ep0_mps = desc->bMaxPacketSize0;

    /* Register standard USB request callbacks */
    usbd_ctrl_xfer_cbs_t cbs;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = get_config_setup;
    if (!usbd_register_request_cb(h, 0x80, USB_REQ_GET_CONFIGURATION, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = get_status_setup;
    if (!usbd_register_request_cb(h, 0x80, USB_REQ_GET_STATUS, &cbs)) return false;
    if (!usbd_register_request_cb(h, 0x82, USB_REQ_GET_STATUS, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = set_clear_feature_setup;
    cbs.status = set_feature_status;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_SET_FEATURE, &cbs)) return false;
    if (!usbd_register_request_cb(h, 0x02, USB_REQ_SET_FEATURE, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = set_clear_feature_setup;
    cbs.status = clear_feature_status;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_CLEAR_FEATURE, &cbs)) return false;
    if (!usbd_register_request_cb(h, 0x02, USB_REQ_CLEAR_FEATURE, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.setup = get_descriptor_setup;
    if (!usbd_register_request_cb(h, 0x80, USB_REQ_GET_DESCRIPTOR, &cbs)) return false;
    if (!usbd_register_request_cb(h, 0x82, USB_REQ_GET_DESCRIPTOR, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.status = set_address_status;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_SET_ADDRESS, &cbs)) return false;

    memset(&cbs, 0, sizeof(cbs));
    cbs.status = set_config_status;
    if (!usbd_register_request_cb(h, 0x00, USB_REQ_SET_CONFIGURATION, &cbs)) return false;

    return h->open(h, speed, sof_en);
}

bool usbd_drv_close(usbd_handle_t *h)
{
    if (!h) return false;
    return h->close(h);
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
        h->remote_wakeup = false;
        h->link_speed = USB_SPEED_UNKNOWN;
        h->config_num = 0;
        h->set_address(h, 0);
        h->endp_open(h, 0x80, USB_ENDP_TYPE_CTRL, h->ep0_mps);
        h->endp_open(h, 0x00, USB_ENDP_TYPE_CTRL, h->ep0_mps);
        h->endp_transfer(h, 0x00, &h->setup, sizeof(usb_setup_t));
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

bool usbd_register_request_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest, usbd_ctrl_xfer_cbs_t *cbs)
{
    if (!h) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->request_cbs); i++)
    {
        usbd_request_cbs_t *req_cbs = &h->request_cbs[i];
        if ((req_cbs->bmRequestType == 0 && req_cbs->bRequest == 0) ||
            (req_cbs->bmRequestType == bmRequestType && req_cbs->bRequest == bRequest))
        {
            req_cbs->bmRequestType = bmRequestType;
            req_cbs->bRequest = bRequest;
            req_cbs->cbs.setup = cbs->setup;
            req_cbs->cbs.data = cbs->data;
            req_cbs->cbs.status = cbs->status;
            return true;
        }
    }
    return false;
}

bool usbd_register_interface_cb(usbd_handle_t *h, void *itf_handle, uint8_t itf_num, usbd_ctrl_xfer_cbs_t *cbs)
{
    if (!h || itf_num >= USB_ARRAY_SIZE(h->interface_cbs)) return false;

    usbd_interface_cbs_t *itf = &h->interface_cbs[itf_num];
    itf->itf_handle = itf_handle;
    itf->cbs.setup = cbs->setup;
    itf->cbs.data = cbs->data;
    itf->cbs.status = cbs->status;
    return true;
}

bool usbd_unregister_request_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest)
{
    if (!h) return false;
    for (size_t i = 0; i < USB_ARRAY_SIZE(h->request_cbs); i++)
    {
        usbd_request_cbs_t *req_cbs = &h->request_cbs[i];
        if (req_cbs->bmRequestType == bmRequestType && req_cbs->bRequest == bRequest)
        {
            req_cbs->bmRequestType = 0;
            req_cbs->bRequest = 0;
            req_cbs->cbs.setup = NULL;
            req_cbs->cbs.data = NULL;
            req_cbs->cbs.status = NULL;
            return true;
        }
    }
    return false;
}

bool usbd_unregister_interface_cb(usbd_handle_t *h, uint8_t interface_num)
{
    if (!h || interface_num >= USB_ARRAY_SIZE(h->interface_cbs)) return false;
    usbd_interface_cbs_t *itf = &h->interface_cbs[interface_num];
    itf->itf_handle = NULL;
    itf->cbs.setup = NULL;
    itf->cbs.data = NULL;
    itf->cbs.status = NULL;
    return true;
}

bool usbd_endp_open(usbd_handle_t *h, const usb_desc_endpoint_t *ep_desc)
{
    if (!h || !ep_desc) return false;
    usb_endp_t endp = ep_desc->bEndpointAddress;
    usb_endp_type_t endp_type = USB_ENDP_GET_TYPE(ep_desc->bmAttributes);
    uint16_t endp_mps = USB_ENDP_GET_MPS(ep_desc->wMaxPacketSize);
    return h->endp_open(h, endp, endp_type, endp_mps);
}

bool usbd_endp_close(usbd_handle_t *h, uint8_t ep_addr)
{
    if (!h) return false;
    return h->endp_close(h, ep_addr);
}
