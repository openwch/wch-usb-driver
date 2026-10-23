/********************************** (C) COPYRIGHT *******************************
* File Name          : cdcd_ecm.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB CDC-ECM device class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "CDCD_ECM"

/* @include */
#include "usb_driver.h"
#include "device/usbd_driver_private.h"

#ifdef USB_CLASS_CDCD_ECM_DRIVER_EN

static bool ctrl_xfer_setup(void *handle, const usb_setup_t *setup, void **buf, size_t *len)
{
    cdcd_ecm_handle_t *h = (cdcd_ecm_handle_t *)handle;

    uint8_t type = USB_GET_REQ_TYPE(setup->bmRequestType);

    if (setup->wIndex == h->data_itf_num && type == USB_REQ_TYPE_STANDARD)
    {
        switch (setup->bRequest)
        {
        case USB_REQ_GET_INTERFACE:
            *buf = &h->alt_setting;
            *len = sizeof(h->alt_setting);
            return true;

        case USB_REQ_SET_INTERFACE:
            *buf = NULL;
            *len = 0;
            return true;
        }
    }
    else if (setup->wIndex == h->ctrl_itf_num && type == USB_REQ_TYPE_CLASS)
    {
        switch (setup->bRequest)
        {
        case CDC_REQ_SEND_ENCAPSULATED_COMMAND:
        case CDC_REQ_GET_ENCAPSULATED_RESPONSE:
        case CDC_REQ_SET_ETHERNET_MULTICAST_FILTERS:
        case CDC_REQ_SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER:
        case CDC_REQ_GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER:
        case CDC_REQ_GET_ETHERNET_STATISTIC:
            break;

        case CDC_REQ_SET_ETHERNET_PACKET_FILTER:
            *buf = NULL;
            *len = 0;
            return true;
        }
    }
    return false;
}

static void ctrl_xfer_status(void *handle, const usb_setup_t *setup, void *buf, size_t len)
{
    cdcd_ecm_handle_t *h = (cdcd_ecm_handle_t *)handle;

    if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_STANDARD)
    {
        switch (setup->bRequest)
        {
        case USB_REQ_SET_INTERFACE:
            h->alt_setting = setup->wValue;
            USB_LOGI("Handle %p: SET_INTERFACE: AltSetting=%d", h, setup->wValue);
            if (h->set_interface_cb)
            {
                h->set_interface_cb(h, setup->wValue);
            }
            break;
        }
    }
    else if (USB_GET_REQ_TYPE(setup->bmRequestType) == USB_REQ_TYPE_CLASS)
    {
        switch (setup->bRequest)
        {
        case CDC_REQ_SET_ETHERNET_PACKET_FILTER:
            USB_LOGI("Handle %p: SET_ETHERNET_PACKET_FILTER: Bitmap=0x%04x", h, setup->wValue);
            if (h->set_ethernet_packet_filter_cb)
            {
                h->set_ethernet_packet_filter_cb(h, setup->wValue);
            }
            break;
        }
    }
}

static void read_callback(void *handle, usb_endp_t endp, void *buf, size_t len)
{
    cdcd_ecm_handle_t *h = (cdcd_ecm_handle_t *)handle;
    if (h->read_comp_cb) h->read_comp_cb(h, buf, len);
}

static void write_callback(void *handle, usb_endp_t endp, void *buf, size_t len)
{
    cdcd_ecm_handle_t *h = (cdcd_ecm_handle_t *)handle;
    if (h->write_comp_cb) h->write_comp_cb(h, buf, len);
}

bool cdcd_ecm_drv_open(cdcd_ecm_handle_t *cdcd_ecm)
{
    if (!cdcd_ecm || !cdcd_ecm->usbd_handle || !cdcd_ecm->notify_ep || !cdcd_ecm->in_ep || !cdcd_ecm->out_ep)
        return false;

    usbd_ctrl_xfer_cbs_t cbs = {
        .setup = ctrl_xfer_setup,
        .data = NULL,
        .status = ctrl_xfer_status,
    };

    if (!usbd_register_interface_cb(cdcd_ecm->usbd_handle, cdcd_ecm, cdcd_ecm->ctrl_itf_num, &cbs)) goto unregister_itf;
    if (!usbd_register_interface_cb(cdcd_ecm->usbd_handle, cdcd_ecm, cdcd_ecm->data_itf_num, &cbs)) goto unregister_itf;
    if (!usbd_endp_open(cdcd_ecm->usbd_handle, cdcd_ecm, cdcd_ecm->notify_ep, NULL)) goto close_notify_ep;
    USB_LOGI("Handle %p: Successfully opened CDC ECM", cdcd_ecm);
    return true;

close_notify_ep:
    usbd_endp_close(cdcd_ecm->usbd_handle, cdcd_ecm->notify_ep->bEndpointAddress);

unregister_itf:
    usbd_unregister_interface_cb(cdcd_ecm->usbd_handle, cdcd_ecm->ctrl_itf_num);
    usbd_unregister_interface_cb(cdcd_ecm->usbd_handle, cdcd_ecm->data_itf_num);
    USB_LOGE("Handle %p: Failed to open CDC ECM", cdcd_ecm);
    return false;
}

bool cdcd_ecm_drv_ctrl_data_endpoint(cdcd_ecm_handle_t *cdcd_ecm, bool enable)
{
    if (!cdcd_ecm || !cdcd_ecm->usbd_handle || !cdcd_ecm->in_ep || !cdcd_ecm->out_ep) return false;

    if (enable)
    {
        if (!usbd_endp_open(cdcd_ecm->usbd_handle, cdcd_ecm, cdcd_ecm->in_ep, write_callback)) goto failed;
        if (!usbd_endp_open(cdcd_ecm->usbd_handle, cdcd_ecm, cdcd_ecm->out_ep, read_callback)) goto failed;
    }
    else
    {
        usbd_endp_close(cdcd_ecm->usbd_handle, cdcd_ecm->in_ep->bEndpointAddress);
        usbd_endp_close(cdcd_ecm->usbd_handle, cdcd_ecm->out_ep->bEndpointAddress);
    }
    return true;

failed:
    usbd_endp_close(cdcd_ecm->usbd_handle, cdcd_ecm->in_ep->bEndpointAddress);
    usbd_endp_close(cdcd_ecm->usbd_handle, cdcd_ecm->out_ep->bEndpointAddress);
    return false;
}

bool cdcd_ecm_drv_connect_change_notify(cdcd_ecm_handle_t *cdcd_ecm, bool connected)
{
    if (!cdcd_ecm || !cdcd_ecm->usbd_handle || !cdcd_ecm->notify_ep) return false;
    if (usbd_endp_is_busy(cdcd_ecm->usbd_handle, cdcd_ecm->notify_ep->bEndpointAddress)) return false;

    usb_setup_t *request = (usb_setup_t *)cdcd_ecm->notify_buf;
    request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
    request->bRequest = CDC_NOTIF_NETWORK_CONNECTION;
    request->wValue = connected ? 0x01 : 0x00;
    request->wIndex = cdcd_ecm->ctrl_itf_num;
    request->wLength = 0;
    return usbd_endp_write(cdcd_ecm->usbd_handle, cdcd_ecm->notify_ep->bEndpointAddress, request, sizeof(usb_setup_t));
}

bool cdcd_ecm_drv_speed_change_notify(cdcd_ecm_handle_t *cdcd_ecm, uint32_t ul_bit_rate, uint32_t dl_bit_rate)
{
    if (!cdcd_ecm || !cdcd_ecm->usbd_handle || !cdcd_ecm->notify_ep) return false;
    if (usbd_endp_is_busy(cdcd_ecm->usbd_handle, cdcd_ecm->notify_ep->bEndpointAddress)) return false;

    usb_setup_t *request = (usb_setup_t *)cdcd_ecm->notify_buf;
    request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
    request->bRequest = CDC_NOTIF_CONNECTION_SPEED_CHANGE;
    request->wValue = 0;
    request->wIndex = cdcd_ecm->ctrl_itf_num;
    request->wLength = sizeof(cdc_connect_speed_change_notify_t);
    cdc_connect_speed_change_notify_t *speed = (cdc_connect_speed_change_notify_t *)&cdcd_ecm->notify_buf[8];
    speed->ULBitRate = ul_bit_rate;
    speed->DLBitRate = dl_bit_rate;
    return usbd_endp_write(cdcd_ecm->usbd_handle, cdcd_ecm->notify_ep->bEndpointAddress, request,
                           sizeof(usb_setup_t) + sizeof(cdc_connect_speed_change_notify_t));
}

bool cdcd_ecm_drv_read(cdcd_ecm_handle_t *cdcd_ecm, void *buf, size_t len)
{
    return usbd_endp_read(cdcd_ecm->usbd_handle, cdcd_ecm->out_ep->bEndpointAddress, buf, len);
}

bool cdcd_ecm_drv_write(cdcd_ecm_handle_t *cdcd_ecm, const void *buf, size_t len)
{
    return usbd_endp_write(cdcd_ecm->usbd_handle, cdcd_ecm->in_ep->bEndpointAddress, buf, len);
}

#endif // USB_CLASS_CDCD_ECM_DRIVER_EN
