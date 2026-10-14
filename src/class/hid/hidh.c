/**
 * @file hidh.c
 * @author Links (lhd@wch.cn)
 * @brief USB HID host class driver
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @define */
#define USB_LOG_TAG "HIDH"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"

#ifdef USB_CLASS_HIDH_DRIVER_EN

/* @define */
#ifndef HIDH_HANDLE_POOL_SIZE
#define HIDH_HANDLE_POOL_SIZE 1
#endif

#define CTRL_XFER_TIMEOUT          100
#define SUPPORTED_PROTOCOL_VERSION 0x0111

/* @enum */
typedef enum
{
    ENUM_STAGE_IDLE,
    ENUM_STAGE_SET_IDLE,
    ENUM_STAGE_GET_DESC,
    ENUM_STAGE_ENUM_FAILED,
    ENUM_STAGE_END,
} enum_stage_t;

/* @struct */
typedef struct hidh_handle
{
    struct hidh_handle *next;
    usbh_device_t *dev;

    /* HID interface information */
    const usb_desc_interface_t *itf_desc;
    const usb_desc_hid_t *hid_desc;
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;

    /* Run-time context */
    uint8_t enum_stage;
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
} hidh_handle_t;

/* @global */
static hidh_handle_t *hid_list = NULL;
static hidh_handle_t handle_pool[HIDH_HANDLE_POOL_SIZE] = {0};
static __attribute__((aligned(4))) uint8_t public_ctrl_buf[1024];

/* @function declarations */
static bool hid_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length);
static bool hid_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num);
static void hid_drv_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);
static void hid_drv_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);

static const usbh_drv_info_t hid_drv_info = {
    .vid = 0x0000,
    .pid = 0x0000,
    .dev_class = 0x00,
    .dev_subclass = 0x00,
    .dev_protocol = 0x00,
    .itf_class = USB_CLASS_HID,
    .itf_subclass = 0x00,
    .itf_protocol = 0x00,
    .match_bitmap = USBH_DRV_MATCH_ITF_CLASS,
    .bind = hid_drv_bind_cb,
    .unbind = hid_drv_unbind_cb,
};

static bool hid_report_desc_parse(hidh_handle_t *hid, const void *buf, size_t len)
{
    return true;
}

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    hidh_handle_t *hid = (hidh_handle_t *)handle;
    if (!rst)
    {
        USB_LOGE("Handle: %p Enumeration control transfer failed at stage %d", hid, hid->enum_stage);
        hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
        return;
    }

    switch (hid->enum_stage)
    {
    case ENUM_STAGE_SET_IDLE:
    {
        hid->enum_stage = ENUM_STAGE_GET_DESC;
        USB_LOGI("Handle: %p SET_IDLE", hid);
        for (size_t i = 0; i < hid->hid_desc->bNumDescriptors; i++)
        {
            if (hid->hid_desc->descriptor[i].bReportType == HID_DESC_REPORT)
            {
                /* Get only the first report descriptor */
                usb_setup_t *request = &hid->ctrl_setup;
                request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
                request->bRequest = USB_REQ_GET_DESCRIPTOR;
                request->wValue = hid->hid_desc->descriptor[i].bReportType << 8;
                request->wIndex = hid->itf_desc->bInterfaceNumber;
                request->wLength = hid->hid_desc->descriptor[i].wReportLength;
                usbh_ctrl_xfer(hid->dev, hid, request, &public_ctrl_buf, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
                return;
            }
        }
        hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
        USB_LOGE("Handle: %p No report descriptor found", hid);
        break;
    }
    case ENUM_STAGE_GET_DESC:
        if (length != setup->wLength)
        {
            hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Get descriptor failed: expected length %d, got %d", hid, setup->wLength, length);
            break;
        }
        if (!hid_report_desc_parse(hid, &public_ctrl_buf, length))
        {
            hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Failed to parse HID report descriptor", hid);
            break;
        }
        if (hid->in_ep && !usbh_endp_open(hid->dev, hid, hid->in_ep, hid_drv_read_cb))
        {
            hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Failed to open IN endpoint", hid);
            break;
        }
        if (hid->out_ep && !usbh_endp_open(hid->dev, hid, hid->out_ep, hid_drv_write_cb))
        {
            hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle: %p Failed to open OUT endpoint", hid);
            if (hid->in_ep) usbh_endp_close(hid->dev, hid->in_ep->bEndpointAddress);
            break;
        }
        hid->enum_stage = ENUM_STAGE_END;
        USB_LOGI("Handle: %p Enumeration completed", hid);
        break;
    }
}

static void hid_enum_task(hidh_handle_t *hid)
{
    switch (hid->enum_stage)
    {
    case ENUM_STAGE_IDLE:
    {
        usb_setup_t *request = &hid->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = HID_REQ_SET_IDLE;
        request->wValue = 0;
        request->wIndex = hid->itf_desc->bInterfaceNumber;
        request->wLength = 0;
        if (usbh_ctrl_xfer(hid->dev, hid, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb))
        {
            hid->enum_stage = ENUM_STAGE_SET_IDLE;
        }
        break;
    }

    case ENUM_STAGE_ENUM_FAILED:
        hid->enum_stage = ENUM_STAGE_END;
        usbh_device_suspend(hid->dev, NULL);
        hid_drv_unbind_cb(hid->dev, hid->itf_desc->bInterfaceNumber);
        break;
    }
}

static void hid_run_task(hidh_handle_t *hid)
{
}

void hidh_drv_task(void)
{
    hidh_handle_t *hid = hid_list;
    while (hid)
    {
        hidh_handle_t *next = hid->next;
        if (!hid->dev->suspended)
        {
            if (hid->enum_stage < ENUM_STAGE_END)
            {
                hid_enum_task(hid);
                break;
            }
            else
            {
                hid_run_task(hid);
            }
        }
        hid = next;
    }
}

bool hidh_drv_open(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_register_driver(h, &hid_drv_info);
}

bool hidh_drv_close(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_unregister_driver(h, &hid_drv_info);
}

static hidh_handle_t *handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(handle_pool); i++)
    {
        hidh_handle_t *hidh = &handle_pool[i];
        if (hidh->dev == NULL)
        {
            memset(hidh, 0, sizeof(hidh_handle_t));
            return hidh;
        }
    }
    return NULL;
}

static bool hid_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length)
{
    if (!dev || dev->speed == USB_SPEED_LOW || !desc || length == 0) return false;

    const usb_desc_interface_t *itf_desc = NULL;
    const usb_desc_hid_t *hid_desc = NULL;
    const usb_desc_endpoint_t *in_ep = NULL;
    const usb_desc_endpoint_t *out_ep = NULL;

    uint8_t endp_count = 0;
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
            if (curr_itf->bNumEndpoints > 2) return false;
            if (curr_itf->bAlternateSetting != 0) break;
            itf_desc = curr_itf;
            endp_count = curr_itf->bNumEndpoints;
            break;
        }

        case USB_DESC_ENDPOINT:
        {
            const usb_desc_endpoint_t *endp_desc = (const usb_desc_endpoint_t *)curr;
            uint8_t endp_dir = USB_ENDP_DIR(endp_desc->bEndpointAddress);
            uint8_t endp_type = USB_ENDP_GET_TYPE(endp_desc->bmAttributes);
            if (!endp_dir || endp_type != USB_ENDP_TYPE_INTR) return false;
            if (!endp_count > 0) break;
            endp_count--;
            const usb_desc_endpoint_t **target_ep = endp_dir ? &in_ep : &out_ep;
            *target_ep = endp_desc;
            break;
        }

        case HID_DESC_HID:
        {
            hid_desc = (const usb_desc_hid_t *)curr;
            if (hid_desc->bcdHID > SUPPORTED_PROTOCOL_VERSION) return false;
            size_t index = 0;
            for (index = 0; index < hid_desc->bNumDescriptors; index++)
            {
                if (hid_desc->descriptor[index].bReportType != HID_DESC_REPORT) continue;
                break;
            }
            /* No report descriptor found */
            if (index >= hid_desc->bNumDescriptors) return false;
        }
        }

        curr += len;
    }

    if (!itf_desc || !hid_desc) return false;

    /* Allocate and initialize a new hid handle */
    hidh_handle_t *hid = handle_alloc();
    if (!hid)
    {
        USB_LOGE("Handle %p: Failed to allocate hid handle", dev);
        return false;
    }

    hid->dev = dev;
    hid->itf_desc = itf_desc;
    hid->hid_desc = hid_desc;
    hid->in_ep = in_ep;
    hid->out_ep = out_ep;
    usbh_list_append((void **)&hid_list, hid);
    return true;
}

static bool hid_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num)
{
    hidh_handle_t *hid = hid_list;
    while (hid)
    {
        hidh_handle_t *next = hid->next;
        if (hid->dev == dev && (itf_num == USBH_DRV_ALL_INTERFACE || hid->itf_desc->bInterfaceNumber == itf_num))
        {
            usbh_list_remove((void **)&hid_list, hid);
            memset(hid, 0, sizeof(hidh_handle_t));
        }
        hid = next;
    }
    return true;
}

static void hid_drv_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
}

static void hid_drv_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
}

#endif // USB_CLASS_HIDH_DRIVER_EN
