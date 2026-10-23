/********************************** (C) COPYRIGHT *******************************
* File Name          : cdch_ecm.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB CDC-ECM host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "CDCH_ECM"

/* @include */
#include <ctype.h>

#include "usb_driver.h"
#include "host/usbh_driver_private.h"

#ifdef USB_CLASS_CDCH_ECM_DRIVER_EN

/* @define */
#ifndef CDCH_ECM_HANDLE_POOL_SIZE
#define CDCH_ECM_HANDLE_POOL_SIZE 1
#endif

#define CTRL_XFER_TIMEOUT          100
#define SUPPORTED_PROTOCOL_VERSION 0x0120

/* @enum */
typedef enum
{
    ENUM_STAGE_IDLE,
    ENUM_STAGE_GET_MAC_ADDR,
    ENUM_STAGE_SET_INTERFACE,
    ENUM_STAGE_SET_ETHERNET_PACKET_FILTER,
    ENUM_STAGE_ENUM_FAILED,
    ENUM_STAGE_END,
} enum_stage_t;

/* @struct */
typedef struct ecmh_handle
{
    struct ecmh_handle *next;
    usbh_device_t *dev;

    /* CDC_ECM interface information */
    const usb_desc_interface_t *ctrl_itf;
    const usb_desc_interface_t *data_enable_itf;
    const usb_desc_interface_t *data_disable_itf;
    const usb_desc_endpoint_t *notify_ep;
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;
    const cdc_desc_ethernet_networking_t *ecm_desc;

    /* Run-time context */
    bool is_open;
    bool target_state;
    bool connected;
    uint8_t enum_stage;
    uint32_t write_timeout;
    uint32_t upload_speed;
    uint32_t download_speed;
    size_t write_len;
    cdch_ecm_open_cb open_cb;
    cdch_ecm_connect_change_cb connect_cb;
    cdch_ecm_speed_change_cb speed_cb;
    cdch_ecm_read_cb read_cb;
    cdch_ecm_write_cb write_cb;
    uint8_t mac_addr[6];
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
    __attribute__((aligned(4))) uint8_t notify_buf[16];
    __attribute__((aligned(4))) uint8_t mac_desc_buf[26];
} ecmh_handle_t;

/* @global */
static ecmh_handle_t *ecm_list = NULL;
static ecmh_handle_t handle_pool[CDCH_ECM_HANDLE_POOL_SIZE] = {0};

/* @function declarations */
static bool ecm_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length);
static bool ecm_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num);
static void ecm_drv_notify_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);
static void ecm_drv_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);
static void ecm_drv_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);

static const usbh_drv_info_t ecm_drv_info = {
    .vid = 0x0000,
    .pid = 0x0000,
    .dev_class = 0x00,
    .dev_subclass = 0x00,
    .dev_protocol = 0x00,
    .itf_class = USB_CLASS_CDC,
    .itf_subclass = CDC_SUBCLASS_ETHERNET_CONTROL_MODEL,
    .itf_protocol = 0x00,
    .match_bitmap = (USBH_DRV_MATCH_ITF_CLASS | USBH_DRV_MATCH_ITF_SUBCLASS),
    .bind = ecm_drv_bind_cb,
    .unbind = ecm_drv_unbind_cb,
};

static bool mac_addr_desc_parse(ecmh_handle_t *ecm, const void *buf, uint16_t length)
{
    const uint8_t *buf_ptr = (const uint8_t *)buf;
    if (buf_ptr[0] != length || buf_ptr[0] != 26 || buf_ptr[1] != USB_DESC_STRING) return false;

    for (size_t i = 0; i < 6; i++)
    {
        uint8_t high = buf_ptr[2 + i * 4];
        uint8_t low = buf_ptr[2 + i * 4 + 2];
        if (!isxdigit(high) || !isxdigit(low)) return false;

        uint8_t high_val = isdigit(high) ? high - '0' : (tolower(high) - 'a' + 10);
        uint8_t low_val = isdigit(low) ? low - '0' : (tolower(low) - 'a' + 10);
        ecm->mac_addr[i] = (high_val << 4) | low_val;
    }

    return true;
}

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    ecmh_handle_t *ecm = (ecmh_handle_t *)handle;
    if (!rst)
    {
        USB_LOGE("Handle %p: Enumeration control transfer failed at stage %d", ecm, ecm->enum_stage);
        ecm->enum_stage = ENUM_STAGE_ENUM_FAILED;
        return;
    }

    switch (ecm->enum_stage)
    {
    case ENUM_STAGE_GET_MAC_ADDR:
    {
        if (!mac_addr_desc_parse(ecm, buf, length))
        {
            USB_LOGE("Handle %p: Failed to parse MAC address descriptor", ecm);
            ecm->enum_stage = ENUM_STAGE_ENUM_FAILED;
            return;
        }

        USB_LOGI("Handle %p: MAC address is %02X:%02X:%02X:%02X:%02X:%02X", ecm, ecm->mac_addr[0], ecm->mac_addr[1],
                 ecm->mac_addr[2], ecm->mac_addr[3], ecm->mac_addr[4], ecm->mac_addr[5]);

        ecm->enum_stage = ENUM_STAGE_SET_INTERFACE;
        usb_setup_t *request = &ecm->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
        request->bRequest = USB_REQ_SET_INTERFACE;
        request->wValue = ecm->data_disable_itf->bAlternateSetting;
        request->wIndex = ecm->data_disable_itf->bInterfaceNumber;
        request->wLength = 0;
        usbh_ctrl_xfer(ecm->dev, ecm, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_SET_INTERFACE:
    {
        ecm->enum_stage = ENUM_STAGE_SET_ETHERNET_PACKET_FILTER;
        usb_setup_t *request = &ecm->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = CDC_REQ_SET_ETHERNET_PACKET_FILTER;
        request->wValue = CDC_ECM_ETH_PACK_FILTER_BROADCAST | CDC_ECM_ETH_PACK_FILTER_DIRECTED |
                          CDC_ECM_ETH_PACK_FILTER_ALL_MULTICAST;
        request->wIndex = ecm->ctrl_itf->bInterfaceNumber;
        request->wLength = 0;
        usbh_ctrl_xfer(ecm->dev, ecm, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_SET_ETHERNET_PACKET_FILTER:
        ecm->enum_stage = ENUM_STAGE_END;
        USB_LOGI("Handle %p: Enumeration completed", ecm);
        break;
    }
}

static void ecm_enum_task(ecmh_handle_t *ecm)
{
    switch (ecm->enum_stage)
    {
    case ENUM_STAGE_IDLE:
    {
        if (usbh_is_ctrl_xfering(ecm->dev)) return;
        ecm->enum_stage = ENUM_STAGE_GET_MAC_ADDR;
        usb_setup_t *request = &ecm->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_DEVICE);
        request->bRequest = USB_REQ_GET_DESCRIPTOR;
        request->wValue = (USB_DESC_STRING << 8) | (ecm->ecm_desc->iMACAddress);
        request->wIndex = ecm->dev->language_id;
        request->wLength = sizeof(ecm->mac_desc_buf);
        usbh_ctrl_xfer(ecm->dev, ecm, request, ecm->mac_desc_buf, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_ENUM_FAILED:
        ecm->enum_stage = ENUM_STAGE_END;
        ecm_drv_unbind_cb(ecm->dev, ecm->ctrl_itf->bInterfaceNumber);
        break;
    }
}

static void run_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    ecmh_handle_t *ecm = (ecmh_handle_t *)handle;
    if (ecm->target_state == ecm->is_open) return;

    if (ecm->is_open)
    {
        ecm->is_open = false;
        usbh_endp_close(ecm->dev, ecm->notify_ep->bEndpointAddress);
        usbh_endp_close(ecm->dev, ecm->in_ep->bEndpointAddress);
        usbh_endp_close(ecm->dev, ecm->out_ep->bEndpointAddress);
        USB_LOGI("Handle %p: Closed", ecm);
    }
    else
    {
        if (rst && !usbh_endp_open(ecm->dev, ecm, ecm->notify_ep, ecm_drv_notify_cb))
        {
            rst = false;
            USB_LOGE("Handle %p: Failed to open notify endpoint", ecm);
        }

        if (rst && !usbh_endp_open(ecm->dev, ecm, ecm->in_ep, ecm_drv_read_cb))
        {
            rst = false;
            usbh_endp_close(ecm->dev, ecm->notify_ep->bEndpointAddress);
            USB_LOGE("Handle %p: Failed to open IN endpoint", ecm);
        }

        if (rst && !usbh_endp_open(ecm->dev, ecm, ecm->out_ep, ecm_drv_write_cb))
        {
            rst = false;
            usbh_endp_close(ecm->dev, ecm->notify_ep->bEndpointAddress);
            usbh_endp_close(ecm->dev, ecm->in_ep->bEndpointAddress);
            USB_LOGE("Handle %p: Failed to open OUT endpoint", ecm);
        }

        ecm->is_open = rst;
        USB_LOGI("Handle %p: Opened: %s", ecm, rst ? "Success" : "Failed");
        if (ecm->open_cb)
        {
            ecm->open_cb(ecm, rst);
        }
    }
    ecm->target_state = ecm->is_open;
}

static void ecm_run_task(ecmh_handle_t *ecm)
{
    if (ecm->target_state != ecm->is_open && !usbh_is_ctrl_xfering(ecm->dev))
    {
        usb_setup_t *request = &ecm->ctrl_setup;
        const usb_desc_interface_t *target_itf = ecm->is_open ? ecm->data_disable_itf : ecm->data_enable_itf;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
        request->bRequest = USB_REQ_SET_INTERFACE;
        request->wValue = target_itf->bAlternateSetting;
        request->wIndex = target_itf->bInterfaceNumber;
        request->wLength = 0;
        usbh_ctrl_xfer(ecm->dev, ecm, request, NULL, CTRL_XFER_TIMEOUT, run_ctrl_xfer_cb);
    }

    if (ecm->is_open && !usbh_is_endp_xfering(ecm->dev, ecm->notify_ep->bEndpointAddress))
    {
        usbh_endp_read(ecm->dev, ecm->notify_ep->bEndpointAddress, ecm->notify_buf, sizeof(ecm->notify_buf), 0);
    }
}

void cdch_ecm_drv_task(void)
{
    ecmh_handle_t *ecm = ecm_list;
    while (ecm)
    {
        ecmh_handle_t *next = ecm->next;
        if (!ecm->dev->suspended)
        {
            if (ecm->enum_stage < ENUM_STAGE_END)
            {
                ecm_enum_task(ecm);
            }
            else
            {
                ecm_run_task(ecm);
            }
        }
        ecm = next;
    }
}

bool cdch_ecm_drv_open(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_register_driver(h, &ecm_drv_info);
}

bool cdch_ecm_drv_close(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_unregister_driver(h, &ecm_drv_info);
}

size_t cdch_ecm_get_count(void)
{
    size_t count = 0;
    ecmh_handle_t *ecm = ecm_list;
    while (ecm)
    {
        if (ecm->enum_stage >= ENUM_STAGE_END) count++;
        ecm = ecm->next;
    }
    return count;
}

ecmh_handle_t *cdch_ecm_get_handle(size_t index)
{
    size_t i = 0;
    ecmh_handle_t *ecm = ecm_list;
    while (ecm)
    {
        if (ecm->enum_stage >= ENUM_STAGE_END)
        {
            if (i == index) return ecm;
            i++;
        }
        ecm = ecm->next;
    }
    return NULL;
}

void cdch_ecm_open(ecmh_handle_t *ecm, cdch_ecm_open_cb open_cb, cdch_ecm_connect_change_cb connect_cb,
                   cdch_ecm_speed_change_cb speed_cb)
{
    if (!ecm)
    {
        if (open_cb) open_cb(ecm, false);
        return;
    }

    ecm->open_cb = open_cb;
    ecm->connect_cb = connect_cb;
    ecm->speed_cb = speed_cb;
    ecm->target_state = true;
}

void cdch_ecm_close(ecmh_handle_t *ecm)
{
    if (!ecm) return;
    ecm->target_state = false;
}

const uint8_t *cdch_ecm_get_mac_addr(ecmh_handle_t *ecm)
{
    if (!ecm) return NULL;
    return ecm->mac_addr;
}

bool cdch_ecm_is_open(ecmh_handle_t *ecm)
{
    if (!ecm) return false;
    return ecm->is_open;
}

bool cdch_ecm_is_connected(ecmh_handle_t *ecm)
{
    if (!ecm) return false;
    return ecm->connected;
}

bool cdch_ecm_get_speed(ecmh_handle_t *ecm, uint32_t *upload_speed, uint32_t *download_speed)
{
    if (!ecm || !upload_speed || !download_speed) return false;
    *upload_speed = ecm->upload_speed;
    *download_speed = ecm->download_speed;
    return true;
}

bool cdch_ecm_read(ecmh_handle_t *ecm, void *buf, size_t length, uint32_t timeout, cdch_ecm_read_cb cb)
{
    if (!ecm || !buf || length == 0 || !ecm->is_open || !ecm->connected) return false;
    if (usbh_is_endp_xfering(ecm->dev, ecm->in_ep->bEndpointAddress)) return false;
    ecm->read_cb = cb;
    usbh_endp_read(ecm->dev, ecm->in_ep->bEndpointAddress, buf, length, timeout);
    return true;
}

bool cdch_ecm_write(ecmh_handle_t *ecm, const void *buf, size_t length, uint32_t timeout, cdch_ecm_write_cb cb)
{
    if (!ecm || !buf || length == 0 || !ecm->is_open || !ecm->connected) return false;
    if (usbh_is_endp_xfering(ecm->dev, ecm->out_ep->bEndpointAddress)) return false;
    ecm->write_cb = cb;
    ecm->write_timeout = timeout;
    usbh_endp_write(ecm->dev, ecm->out_ep->bEndpointAddress, buf, length, timeout);
    return true;
}

static ecmh_handle_t *handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(handle_pool); i++)
    {
        ecmh_handle_t *ecm = &handle_pool[i];
        if (ecm->dev == NULL)
        {
            memset(ecm, 0, sizeof(ecmh_handle_t));
            return ecm;
        }
    }
    return NULL;
}

static bool ecm_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length)
{
    if (!dev || dev->speed == USB_SPEED_LOW || !desc || length == 0) return false;

    const usb_desc_interface_t *ctrl_itf = NULL;
    const usb_desc_interface_t *data_enable_itf = NULL;
    const usb_desc_interface_t *data_disable_itf = NULL;
    const usb_desc_endpoint_t *notify_ep = NULL;
    const usb_desc_endpoint_t *in_ep = NULL;
    const usb_desc_endpoint_t *out_ep = NULL;
    const cdc_desc_ethernet_networking_t *ecm_desc = NULL;

    uint8_t curr_itf_type = 0;
    uint8_t endp_count = 0;
    const cdc_desc_union_t *union_desc = NULL;
    const uint8_t *curr = (const uint8_t *)desc;
    const uint8_t *end = curr + length;

    while (curr < end)
    {
        uint8_t len = curr[0];
        uint8_t type = curr[1];

        switch (type)
        {
        case USB_DESC_INTERFACE:
        {
            const usb_desc_interface_t *curr_itf = (const usb_desc_interface_t *)curr;
            if (curr_itf->bInterfaceClass == USB_CLASS_CDC)
            {
                if (curr_itf->bNumEndpoints > 1) return false;
                if (curr_itf->bAlternateSetting != 0) break;
                ctrl_itf = curr_itf;
                endp_count = curr_itf->bNumEndpoints;
                curr_itf_type = USB_CLASS_CDC;
            }
            else if (curr_itf->bInterfaceClass == USB_CLASS_CDC_DATA)
            {
                if (!union_desc || union_desc->bSubordinateInterface[0] != curr_itf->bInterfaceNumber) return false;
                if (curr_itf->bNumEndpoints > 2) return false;
                if (curr_itf->bAlternateSetting == 0 && curr_itf->bNumEndpoints == 0) data_disable_itf = curr_itf;
                if (curr_itf->bAlternateSetting > 0 && curr_itf->bNumEndpoints == 2) data_enable_itf = curr_itf;
                endp_count = curr_itf->bNumEndpoints;
                curr_itf_type = USB_CLASS_CDC_DATA;
            }
            else
            {
                return false;
            }
            break;
        }

        case USB_DESC_ENDPOINT:
        {
            const usb_desc_endpoint_t *endp_desc = (const usb_desc_endpoint_t *)curr;
            uint8_t endp_dir = USB_ENDP_DIR(endp_desc->bEndpointAddress);
            uint8_t endp_type = USB_ENDP_GET_TYPE(endp_desc->bmAttributes);
            if (curr_itf_type == USB_CLASS_CDC)
            {
                if (!endp_dir || endp_type != USB_ENDP_TYPE_INTR) return false;
                if (!endp_count) break;
                endp_count--;
                notify_ep = endp_desc;
            }
            else if (curr_itf_type == USB_CLASS_CDC_DATA)
            {
                if (endp_type != USB_ENDP_TYPE_BULK) return false;
                if (!endp_count) break;
                endp_count--;
                const usb_desc_endpoint_t **target_ep = endp_dir ? &in_ep : &out_ep;
                *target_ep = endp_desc;
            }
            break;
        }

        case USB_DESC_CS_INTERFACE:
        {
            uint8_t class_type = curr[2];
            if (class_type == CDC_DESC_HEADER)
            {
                cdc_desc_header_t *header_desc = (cdc_desc_header_t *)curr;
                if (header_desc->bLength < sizeof(cdc_desc_header_t)) return false;
                if (header_desc->bcdCDC > SUPPORTED_PROTOCOL_VERSION) return false;
            }
            else if (class_type == CDC_DESC_UNION)
            {
                union_desc = (cdc_desc_union_t *)curr;
                if (union_desc->bLength < sizeof(cdc_desc_union_t) + 1) return false;
                if (union_desc->bControlInterface != ctrl_itf->bInterfaceNumber) return false;
            }
            else if (class_type == CDC_DESC_ETHERNET_NETWORKING)
            {
                ecm_desc = (cdc_desc_ethernet_networking_t *)curr;
                if (ecm_desc->bLength < sizeof(cdc_desc_ethernet_networking_t)) return false;
                if (!ecm_desc->iMACAddress) return false;
            }
            break;
        }
        }

        curr += len;
    }

    if (!ctrl_itf || !data_enable_itf || !data_disable_itf || !notify_ep || !in_ep || !out_ep || !ecm_desc)
        return false;

    /* Allocate and initialize a new cdc ecm handle */
    ecmh_handle_t *ecm = handle_alloc();
    if (!ecm)
    {
        USB_LOGE("Handle %p: Failed to allocate cdc ecm handle", dev);
        return false;
    }

    ecm->dev = dev;
    ecm->ctrl_itf = ctrl_itf;
    ecm->data_enable_itf = data_enable_itf;
    ecm->data_disable_itf = data_disable_itf;
    ecm->notify_ep = notify_ep;
    ecm->in_ep = in_ep;
    ecm->out_ep = out_ep;
    ecm->ecm_desc = ecm_desc;
    usbh_list_append((void **)&ecm_list, ecm);
    return true;
}

static bool ecm_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num)
{
    ecmh_handle_t *ecm = ecm_list;
    while (ecm)
    {
        ecmh_handle_t *next = ecm->next;
        if (ecm->dev == dev && (itf_num == USBH_DRV_ALL_INTERFACE || ecm->ctrl_itf->bInterfaceNumber == itf_num))
        {
            usbh_endp_close(ecm->dev, ecm->notify_ep->bEndpointAddress);
            usbh_endp_close(ecm->dev, ecm->in_ep->bEndpointAddress);
            usbh_endp_close(ecm->dev, ecm->out_ep->bEndpointAddress);
            usbh_list_remove((void **)&ecm_list, ecm);
            memset(ecm, 0, sizeof(ecmh_handle_t));
        }
        ecm = next;
    }
    return true;
}

static void ecm_drv_notify_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    if (!rst || len < sizeof(usb_setup_t)) return;

    ecmh_handle_t *ecm = (ecmh_handle_t *)handle;
    usb_setup_t *setup = (usb_setup_t *)ecm->notify_buf;

    if (setup->bmRequestType != USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE)) return;

    switch (setup->bRequest)
    {
    case CDC_NOTIF_NETWORK_CONNECTION:
        ecm->connected = setup->wValue ? true : false;
        USB_LOGI("Handle %p: Network connection status is %s", ecm, setup->wValue ? "connected" : "disconnected");
        if (ecm->connect_cb)
        {
            ecm->connect_cb(ecm, setup->wValue ? true : false);
        }
        break;

    case CDC_NOTIF_CONNECTION_SPEED_CHANGE:
    {
        if (len < sizeof(usb_setup_t) + sizeof(cdc_connect_speed_change_notify_t)) return;
        cdc_connect_speed_change_notify_t *notify = (cdc_connect_speed_change_notify_t *)(ecm->notify_buf + 8);
        ecm->upload_speed = notify->ULBitRate;
        ecm->download_speed = notify->DLBitRate;
        USB_LOGI("Handle %p: Connection speed changed: upload %u bps, download %u bps", ecm, notify->ULBitRate,
                 notify->DLBitRate);
        if (ecm->speed_cb)
        {
            ecm->speed_cb(ecm, notify->ULBitRate, notify->DLBitRate);
        }
        break;
    }
    }
}

static void ecm_drv_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    ecmh_handle_t *ecm = (ecmh_handle_t *)handle;
    if (rst && ecm->is_open && ecm->connected)
    {
        if (ecm->read_cb)
        {
            ecm->read_cb(ecm, buf, len);
        }
    }
}

static void ecm_drv_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    ecmh_handle_t *ecm = (ecmh_handle_t *)handle;
    if (rst && ecm->is_open && ecm->connected)
    {
        if (len && len % USB_ENDP_GET_MPS(ecm->out_ep->wMaxPacketSize) == 0)
        {
            ecm->write_len = len;
            usbh_endp_write(ecm->dev, ecm->out_ep->bEndpointAddress, buf, 0, ecm->write_timeout);
        }
        else
        {
            if (ecm->write_cb)
            {
                ecm->write_cb(ecm, buf, len ? len : ecm->write_len);
            }
        }
    }
}

#endif // USB_CLASS_CDCH_ECM_DRIVER_EN
