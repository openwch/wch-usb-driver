/**
 * @file cdcd_acm.c
 * @author Links (lhd@wch.cn)
 * @brief USB CDC-ACM device class driver
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @define */
#define USB_LOG_TAG "CDCD_ACM"

/* @include */
#include "usb_driver.h"
#include "device/usbd_driver_private.h"

#ifdef USB_CLASS_CDCD_ACM_DRIVER_EN

static bool ctrl_xfer_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    cdcd_acm_handle_t *h = (cdcd_acm_handle_t *)handle;

    if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_CLASS)
    {
        switch (setup->bRequest)
        {
        case CDC_CLASS_REQ_SET_LINE_CODING:
        case CDC_CLASS_REQ_GET_LINE_CODING:
            *buf = &h->line_coding;
            *len = sizeof(h->line_coding);
            return true;

        case CDC_CLASS_REQ_SET_CONTROL_LINE_STATE:
        case CDC_CLASS_REQ_SEND_BREAK:
            return true;
        }
    }
    return false;
}

static bool ctrl_xfer_data(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    cdcd_acm_handle_t *h = (cdcd_acm_handle_t *)handle;

    if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_CLASS)
    {
        switch (setup->bRequest)
        {
        case CDC_CLASS_REQ_SET_LINE_CODING:
            USB_LOGI("SET_LINE_CODING: Bit_Rate=%d, Stop_Bits=%d, Parity=%d, Data_Bits=%d", h->line_coding.bit_rate,
                     h->line_coding.stop_bits, h->line_coding.parity, h->line_coding.data_bits);
            if (h->set_line_coding_cb)
            {
                return h->set_line_coding_cb(h, &h->line_coding);
            }
        }
    }
    return true;
}

static void ctrl_xfer_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    cdcd_acm_handle_t *h = (cdcd_acm_handle_t *)handle;

    if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_CLASS)
    {
        switch (setup->bRequest)
        {
        case CDC_CLASS_REQ_SET_CONTROL_LINE_STATE:
            USB_LOGI("SET_CONTROL_LINE_STATE: Bitmap=0x%04x", setup->wValue);
            if (h->set_control_line_state_cb)
            {
                h->set_control_line_state_cb(h, setup->wValue);
            }
            break;

        case CDC_CLASS_REQ_SEND_BREAK:
            USB_LOGI("SEND_BREAK: Duration=0x%04x", setup->wValue);
            if (h->send_break_cb)
            {
                h->send_break_cb(h, setup->wValue);
            }
            break;
        }
    }
}

static void read_callback(void *handle, usb_endp_t endp, void *buf, size_t len)
{
    cdcd_acm_handle_t *h = (cdcd_acm_handle_t *)handle;
    if (h->read_comp_cb) h->read_comp_cb(h, buf, len);
}

static void write_callback(void *handle, usb_endp_t endp, void *buf, size_t len)
{
    cdcd_acm_handle_t *h = (cdcd_acm_handle_t *)handle;
    if (h->write_comp_cb) h->write_comp_cb(h, buf, len);
}

bool cdcd_acm_drv_open(cdcd_acm_handle_t *cdcd_acm)
{
    if (!cdcd_acm || !cdcd_acm->usbd_handle || !cdcd_acm->notify_ep || !cdcd_acm->in_ep || !cdcd_acm->out_ep)
        return false;

    usbd_ctrl_xfer_cbs_t cbs = {
        .setup = ctrl_xfer_setup,
        .data = ctrl_xfer_data,
        .status = ctrl_xfer_status,
    };

    if (!usbd_register_interface_cb(cdcd_acm->usbd_handle, cdcd_acm, cdcd_acm->ctrl_itf_num, &cbs)) goto unregister_itf;
    if (!usbd_endp_open(cdcd_acm->usbd_handle, cdcd_acm, cdcd_acm->notify_ep, NULL)) goto close_notify_ep;
    if (!usbd_endp_open(cdcd_acm->usbd_handle, cdcd_acm, cdcd_acm->in_ep, write_callback)) goto close_in_ep;
    if (!usbd_endp_open(cdcd_acm->usbd_handle, cdcd_acm, cdcd_acm->out_ep, read_callback)) goto close_out_ep;
    return true;

close_out_ep:
    usbd_endp_close(cdcd_acm->usbd_handle, cdcd_acm->out_ep->bEndpointAddress);

close_in_ep:
    usbd_endp_close(cdcd_acm->usbd_handle, cdcd_acm->in_ep->bEndpointAddress);

close_notify_ep:
    usbd_endp_close(cdcd_acm->usbd_handle, cdcd_acm->notify_ep->bEndpointAddress);

unregister_itf:
    usbd_unregister_interface_cb(cdcd_acm->usbd_handle, cdcd_acm->ctrl_itf_num);
    return false;
}

bool cdcd_acm_drv_read(cdcd_acm_handle_t *cdcd_acm, void *buf, size_t len)
{
    return usbd_endp_read(cdcd_acm->usbd_handle, cdcd_acm->out_ep->bEndpointAddress, buf, len);
}

bool cdcd_acm_drv_write(cdcd_acm_handle_t *cdcd_acm, const void *buf, size_t len)
{
    return usbd_endp_write(cdcd_acm->usbd_handle, cdcd_acm->in_ep->bEndpointAddress, buf, len);
}

#endif // USB_CLASS_CDCD_ACM_DRIVER_EN
