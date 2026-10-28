/********************************** (C) COPYRIGHT *******************************
* File Name          : uacd.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB UAC device class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "UACD"

/* @include */
#include "usb_driver.h"
#include "device/usbd_driver_private.h"

#ifdef USB_CLASS_UACD_DRIVER_EN

static bool ctrl_xfer_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    uacd_handle_t *h = (uacd_handle_t *)handle;

    uint8_t type = USB_GET_REQ_TYPE(setup->bmRequestType);

    if (type == USB_REQ_TYPE_STANDARD)
    {
        switch (setup->bRequest)
        {
        case USB_REQ_GET_INTERFACE:
            if (h->get_itf_cb)
            {
                h->temp_buf[0] = 0;
                h->get_itf_cb(h, setup->wIndex, h->temp_buf);
                *buf = h->temp_buf;
                *len = sizeof(uint8_t);
                return true;
            }
            break;

        case USB_REQ_SET_INTERFACE:
            if (h->set_itf_cb)
            {
                h->set_itf_cb(h, setup->wIndex, setup->wValue);
                return true;
            }
            break;
        }
    }
    else if (type == USB_REQ_TYPE_CLASS)
    {
        *buf = NULL;
        *len = 0;
        uint8_t rcpt = USB_GET_REQ_RCPT(setup->bmRequestType);
        uint8_t id_ep = rcpt == USB_REQ_RCPT_INTERFACE ? USB_U16_HIGH(setup->wIndex) : USB_U16_LOW(setup->wIndex);

        switch (setup->bRequest)
        {
        case UAC_REQ_SET_CUR:
        case UAC_REQ_SET_MIN:
        case UAC_REQ_SET_MAX:
        case UAC_REQ_SET_RES:
            *buf = h->temp_buf;
            *len = sizeof(h->temp_buf);
            break;

        case UAC_REQ_GET_CUR:
            if (h->get_cur_cb)
            {
                h->get_cur_cb(h, rcpt, id_ep, setup->wValue, buf, len);
            }
            break;

        case UAC_REQ_GET_MIN:
            if (h->get_min_cb)
            {
                h->get_min_cb(h, rcpt, id_ep, setup->wValue, buf, len);
            }
            break;

        case UAC_REQ_GET_MAX:
            if (h->get_max_cb)
            {
                h->get_max_cb(h, rcpt, id_ep, setup->wValue, buf, len);
            }
            break;

        case UAC_REQ_GET_RES:
            if (h->get_res_cb)
            {
                h->get_res_cb(h, rcpt, id_ep, setup->wValue, buf, len);
            }
            break;
        }

        return (*buf && *len) ? true : false;
    }

    return false;
}

static void ctrl_xfer_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    uacd_handle_t *h = (uacd_handle_t *)handle;

    if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_CLASS)
    {
        uint8_t rcpt = USB_GET_REQ_RCPT(setup->bmRequestType);
        uint8_t id_ep = rcpt == USB_REQ_RCPT_INTERFACE ? USB_U16_HIGH(setup->wIndex) : USB_U16_LOW(setup->wIndex);

        switch (setup->bRequest)
        {
        case UAC_REQ_SET_CUR:
            if (h->set_cur_cb)
            {
                h->set_cur_cb(h, rcpt, id_ep, setup->wValue, buf, len);
            }
            break;

        case UAC_REQ_SET_MIN:
            if (h->set_min_cb)
            {
                h->set_min_cb(h, rcpt, id_ep, setup->wValue, buf, len);
            }
            break;

        case UAC_REQ_SET_MAX:
            if (h->set_max_cb)
            {
                h->set_max_cb(h, rcpt, id_ep, setup->wValue, buf, len);
            }
            break;

        case UAC_REQ_SET_RES:
            if (h->set_res_cb)
            {
                h->set_res_cb(h, rcpt, id_ep, setup->wValue, buf, len);
            }
            break;
        }
    }
}

static usbd_ctrl_xfer_cbs_t ctrl_xfer_cbs = {
    .setup = ctrl_xfer_setup,
    .data = NULL,
    .status = ctrl_xfer_status,
};

static void read_callback(void *handle, usb_endp_t endp, void *buf, size_t len)
{
    uacd_handle_t *h = (uacd_handle_t *)handle;
    if (h->read_comp_cb) h->read_comp_cb(h, endp, buf, len);
}

static void write_callback(void *handle, usb_endp_t endp, void *buf, size_t len)
{
    uacd_handle_t *h = (uacd_handle_t *)handle;
    if (h->write_comp_cb) h->write_comp_cb(h, endp, buf, len);
}

bool uacd_drv_open(uacd_handle_t *uacd)
{
    if (!uacd || !uacd->usbd_handle) return false;

    if (!usbd_register_itf_req_cb(uacd->usbd_handle, uacd, uacd->ctrl_itf_num, &ctrl_xfer_cbs)) goto unregister_itf;
    USB_LOGI("Handle %p: Successfully opened UAC", uacd);
    return true;

unregister_itf:
    usbd_unregister_itf_req_cb(uacd->usbd_handle, uacd->ctrl_itf_num);
    USB_LOGE("Handle %p: Failed to open UAC", uacd);
    return false;
}

bool uacd_drv_stream_interface_append(uacd_handle_t *uacd, uint8_t itf_num)
{
    if (!uacd || !uacd->usbd_handle) return false;

    if (usbd_register_itf_req_cb(uacd->usbd_handle, uacd, itf_num, &ctrl_xfer_cbs))
    {
        USB_LOGI("Handle %p: Successfully appended UAC stream interface %d", uacd, itf_num);
        return true;
    }
    USB_LOGE("Handle %p: Failed to append UAC stream interface %d", uacd, itf_num);
    return false;
}

bool uacd_drv_stream_endpoint_append(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc)
{
    if (!uacd || !uacd->usbd_handle || !desc) return false;

    if (usbd_register_endp_req_cb(uacd->usbd_handle, uacd, desc->bEndpointAddress, &ctrl_xfer_cbs))
    {
        USB_LOGI("Handle %p: Successfully appended UAC stream endpoint %02x", uacd, desc->bEndpointAddress);
        return true;
    }
    USB_LOGE("Handle %p: Failed to append UAC stream endpoint %02x", uacd, desc->bEndpointAddress);
    return false;
}

bool uacd_drv_open_stream_endpoint(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc)
{
    if (!uacd || !uacd->usbd_handle || !desc) return false;
    return usbd_endp_open(uacd->usbd_handle, uacd, (const usb_desc_endpoint_t *)desc,
                          USB_ENDP_DIR(desc->bEndpointAddress) ? write_callback : read_callback);
}

bool uacd_drv_close_stream_endpoint(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc)
{
    if (!uacd || !uacd->usbd_handle || !desc) return false;
    return usbd_endp_close(uacd->usbd_handle, desc->bEndpointAddress);
}

bool uacd_drv_read(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc, void *buf, size_t len)
{
    return usbd_endp_read(uacd->usbd_handle, desc->bEndpointAddress, buf, len);
}

bool uacd_drv_write(uacd_handle_t *uacd, const uac_desc_audio_endpoint_t *desc, const void *buf, size_t len)
{
    return usbd_endp_write(uacd->usbd_handle, desc->bEndpointAddress, buf, len);
}

#endif // USB_CLASS_UACD_DRIVER_EN
