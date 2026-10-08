/**
 * @file hidd.c
 * @author Links (lhd@wch.cn)
 * @brief USB HID device class driver
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include "usb_driver.h"
#include "device/usbd_driver_private.h"

static bool ctrl_xfer_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    hidd_handle_t *h = (hidd_handle_t *)handle;
    uint8_t req_type = USB_GET_REQ_TYPE(setup->bmRequestType);

    if (req_type == USB_REQ_TYPE_STANDARD)
    {
        switch (setup->bRequest)
        {
        case USB_REQ_GET_DESCRIPTOR:
            if (h->get_desc_cb)
            {
                return h->get_desc_cb(h, USB_U16_HIGH(setup->wValue), USB_U16_LOW(setup->wValue), buf, len);
            }
            break;

        case USB_REQ_SET_DESCRIPTOR:
            if (h->set_desc_cb)
            {
                return h->set_desc_cb(h, USB_U16_HIGH(setup->wValue), USB_U16_LOW(setup->wValue), buf, len);
            }
            break;
        }
    }
    else if (req_type == USB_REQ_TYPE_CLASS)
    {
        switch (setup->bRequest)
        {
        case HID_REQ_GET_IDLE:
        case HID_REQ_GET_PROTOCOL:
            h->ctrl_req_buf = setup->bRequest == HID_REQ_GET_IDLE ? h->idle_rate[USB_U16_LOW(setup->wValue)]
                                                                  : h->protocol;
            *buf = &h->ctrl_req_buf;
            *len = 1;
            return true;

        case HID_REQ_GET_REPORT:
        case HID_REQ_SET_REPORT:
            if ((setup->bRequest == HID_REQ_GET_REPORT && h->get_report_prev_cb) ||
                (setup->bRequest == HID_REQ_SET_REPORT && h->set_report_prev_cb))
            {
                *buf = h->report_buf;
                uint8_t report_type = USB_U16_HIGH(setup->wValue);
                uint8_t report_id = USB_U16_LOW(setup->wValue);
                return setup->bRequest == HID_REQ_GET_REPORT
                           ? h->get_report_prev_cb(h, report_type, report_id, setup->wLength, len)
                           : h->set_report_prev_cb(h, report_type, report_id, setup->wLength, len);
            }
            else if (h->report_buf && setup->wLength <= h->report_buf_size)
            {
                *buf = h->report_buf;
                *len = setup->wLength;
                return true;
            }
            else
            {
                return false;
            }

        case HID_REQ_SET_IDLE:
        case HID_REQ_SET_PROTOCOL:
            return true;
        }
    }
    return false;
}

static void ctrl_xfer_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    hidd_handle_t *h = (hidd_handle_t *)handle;
    uint8_t req_type = USB_GET_REQ_TYPE(setup->bmRequestType);

    if (req_type == USB_REQ_TYPE_CLASS)
    {
        switch (setup->bRequest)
        {
        case HID_REQ_GET_REPORT:
            if (h->get_report_comp_cb)
            {
                uint8_t report_type = USB_U16_HIGH(setup->wValue);
                uint8_t report_id = USB_U16_LOW(setup->wValue);
                h->get_report_comp_cb(h, report_type, report_id, buf, len);
            }
            break;

        case HID_REQ_SET_REPORT:
            if (h->set_report_comp_cb)
            {
                uint8_t report_type = USB_U16_HIGH(setup->wValue);
                uint8_t report_id = USB_U16_LOW(setup->wValue);
                h->set_report_comp_cb(h, report_type, report_id, buf, len);
            }
            break;

        case HID_REQ_SET_IDLE:
        {
            uint8_t report_id = USB_U16_LOW(setup->wValue);
            uint8_t idle_rate = USB_U16_HIGH(setup->wValue);
            h->idle_rate[report_id] = idle_rate;
            if (h->set_idle_cb)
            {
                h->set_idle_cb(h, report_id, idle_rate);
            }
            break;
        }

        case HID_REQ_SET_PROTOCOL:
        {
            uint8_t protocol = USB_U16_LOW(setup->wValue);
            h->protocol = protocol;
            if (h->set_protocol_cb)
            {
                h->set_protocol_cb(h, protocol);
            }
            break;
        }
        }
    }
}

static void read_callback(void *handle, usb_endp_t endp, void *buf, size_t len)
{
    hidd_handle_t *h = (hidd_handle_t *)handle;
    if (h->read_comp_cb)
    {
        h->read_comp_cb(h, buf, len);
    }
}

static void write_callback(void *handle, usb_endp_t endp, void *buf, size_t len)
{
    hidd_handle_t *h = (hidd_handle_t *)handle;
    if (h->write_comp_cb)
    {
        h->write_comp_cb(h, buf, len);
    }
}

bool hidd_drv_open(hidd_handle_t *hidd)
{
    if (!hidd || !hidd->usbd_handle || !hidd->get_desc_cb) return false;

    usbd_ctrl_xfer_cbs_t cbs = {
        .setup = ctrl_xfer_setup,
        .data = NULL,
        .status = ctrl_xfer_status,
    };

    if (!usbd_register_interface_cb(hidd->usbd_handle, hidd, hidd->itf_num, &cbs)) goto unregister_interface;
    if (hidd->in_ep)
    {
        if (!usbd_endp_open(hidd->usbd_handle, hidd, hidd->in_ep, write_callback)) goto close_in_ep;
    }
    if (hidd->out_ep)
    {
        if (!usbd_endp_open(hidd->usbd_handle, hidd, hidd->out_ep, read_callback)) goto close_out_ep;
    }
    return true;

close_out_ep:
    if (hidd->out_ep)
    {
        usbd_endp_close(hidd->usbd_handle, hidd->out_ep->bEndpointAddress);
    }

close_in_ep:
    if (hidd->in_ep)
    {
        usbd_endp_close(hidd->usbd_handle, hidd->in_ep->bEndpointAddress);
    }

unregister_interface:
    usbd_unregister_interface_cb(hidd->usbd_handle, hidd->itf_num);
    return false;
}

bool hidd_drv_read(hidd_handle_t *hidd, void *buf, size_t len)
{
    return usbd_endp_read(hidd->usbd_handle, hidd->out_ep->bEndpointAddress, buf, len);
}

bool hidd_drv_write(hidd_handle_t *hidd, const void *buf, size_t len)
{
    return usbd_endp_write(hidd->usbd_handle, hidd->in_ep->bEndpointAddress, buf, len);
}
