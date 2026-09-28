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

/* @function declaration */
static bool ctrl_xfer_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len);
static void ctrl_xfer_status(void *handle, const usb_setup_t *setup, void *buf, size_t len);

bool hidd_drv_open(usbd_handle_t *usbd, hidd_handle_t *hidd, const hidd_info_t *info)
{
    if (!usbd || !hidd || !info || !info->hid_desc) return false;

    memset(hidd, 0, sizeof(hidd_handle_t));
    hidd->usbd_handle = usbd;
    hidd->info = info;

    usbd_ctrl_xfer_cbs_t cbs = {
        .setup = ctrl_xfer_setup,
        .data = NULL,
        .status = ctrl_xfer_status,
    };

    if (!usbd_register_interface_cb(usbd, hidd, info->itf_num, &cbs)) goto unregister_interface;
    if (info->in_ep)
    {
        if (!usbd_endp_open(usbd, info->in_ep)) goto close_in_ep;
    }
    if (info->out_ep)
    {
        if (!usbd_endp_open(usbd, info->out_ep)) goto close_out_ep;
    }

    return true;

close_out_ep:
    usbd_endp_close(usbd, info->out_ep->bEndpointAddress);

close_in_ep:
    usbd_endp_close(usbd, info->in_ep->bEndpointAddress);

unregister_interface:
    usbd_unregister_interface_cb(usbd, info->itf_num);

    return false;
}

static bool ctrl_xfer_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    hidd_handle_t *h = (hidd_handle_t *)handle;
    uint8_t req_type = USB_GET_REQ_TYPE(setup->bmRequestType);

    if (req_type == USB_REQ_TYPE_STANDARD && setup->bRequest == USB_REQ_GET_DESCRIPTOR)
    {
        switch (setup->wValue >> 8)
        {
        case HID_DESC_HID:
            if (h->info->hid_desc)
            {
                *buf = (void *)h->info->hid_desc;
                *len = sizeof(hid_desc_t);
                return true;
            }
            break;

        case HID_DESC_REPORT:
            if (h->info->report_desc && h->info->report_desc_size)
            {
                *buf = (void *)h->info->report_desc;
                *len = h->info->report_desc_size;
                return true;
            }
            break;

        case HID_DESC_PHYSICAL:
            if (h->info->phy_desc && h->info->phy_desc_size)
            {
                *buf = (void *)h->info->phy_desc;
                *len = h->info->phy_desc_size;
                return true;
            }
            break;
        }
    }
    else if (req_type == USB_REQ_TYPE_CLASS)
    {
        switch (setup->bRequest)
        {
        case HID_REQ_GET_REPORT:
            if (h->info->get_report_prev_cb)
            {
                uint8_t report_type = USB_U16_HIGH(setup->wValue);
                uint8_t report_id = USB_U16_LOW(setup->wValue);
                return h->info->get_report_prev_cb(h, report_type, report_id, setup->wLength, buf, len);
            }
            break;

        case HID_REQ_GET_IDLE:
            h->ctrl_req_buf = h->idle_rate[USB_U16_LOW(setup->wValue)];
            *buf = &h->ctrl_req_buf;
            *len = sizeof(h->ctrl_req_buf);
            return true;

        case HID_REQ_GET_PROTOCOL:
            *buf = &h->protocol;
            *len = sizeof(h->protocol);
            return true;

        case HID_REQ_SET_REPORT:
            if (h->info->set_report_prev_cb)
            {
                uint8_t report_type = USB_U16_HIGH(setup->wValue);
                uint8_t report_id = USB_U16_LOW(setup->wValue);
                return h->info->set_report_prev_cb(h, report_type, report_id, setup->wLength, buf, len);
            }
            break;

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
            if (h->info->get_report_comp_cb)
            {
                uint8_t report_type = USB_U16_HIGH(setup->wValue);
                uint8_t report_id = USB_U16_LOW(setup->wValue);
                h->info->get_report_comp_cb(h, report_type, report_id, buf, len);
            }
            break;

        case HID_REQ_SET_REPORT:
            if (h->info->set_report_comp_cb)
            {
                uint8_t report_type = USB_U16_HIGH(setup->wValue);
                uint8_t report_id = USB_U16_LOW(setup->wValue);
                h->info->set_report_comp_cb(h, report_type, report_id, buf, len);
            }
            break;

        case HID_REQ_SET_IDLE:
        {
            uint8_t report_id = USB_U16_LOW(setup->wValue);
            uint8_t idle_rate = USB_U16_HIGH(setup->wValue);
            h->idle_rate[report_id] = idle_rate;
            if (h->info->set_idle_cb)
            {
                h->info->set_idle_cb(h, report_id, idle_rate);
            }
            break;
        }

        case HID_REQ_SET_PROTOCOL:
        {
            uint8_t protocol = USB_U16_LOW(setup->wValue);
            h->protocol = protocol;
            if (h->info->set_protocol_cb)
            {
                h->info->set_protocol_cb(h, protocol);
            }
            break;
        }
        }
    }
}
