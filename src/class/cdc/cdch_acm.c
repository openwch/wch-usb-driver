/********************************** (C) COPYRIGHT *******************************
* File Name          : cdch_acm.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB CDC-ACM host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "CDCH_ACM"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"

#ifdef USB_CLASS_CDCH_ACM_DRIVER_EN

/* @define */
#ifndef CDCH_ACM_HANDLE_POOL_SIZE
#define CDCH_ACM_HANDLE_POOL_SIZE 1
#endif

#define CTRL_XFER_TIMEOUT          100
#define SUPPORTED_PROTOCOL_VERSION 0x0120

/* @enum */
typedef enum
{
    ENUM_STAGE_IDLE,
    ENUM_STAGE_GET_LINE_CODING,
    ENUM_STAGE_SET_CONTROL_LINE_STATE,
    ENUM_STAGE_ENUM_FAILED,
    ENUM_STAGE_END,
} enum_stage_t;

/* @struct */
typedef struct acmh_handle
{
    struct acmh_handle *next;
    usbh_device_t *dev;

    /* CDC_ACM interface information */
    const usb_desc_interface_t *ctrl_itf;
    const usb_desc_interface_t *data_itf;
    const usb_desc_endpoint_t *notify_ep;
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;
    const cdc_desc_abstract_control_management_t *acm_desc;

    /* Run-time context */
    bool is_open;
    bool opening;
    bool line_state_changed;
    bool break_changed;
    uint8_t enum_stage;
    uint16_t line_state;
    uint16_t duration;
    cdch_acm_open_cb open_cb;
    cdch_acm_read_cb read_cb;
    cdch_acm_write_cb write_cb;
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
    __attribute__((aligned(4))) cdc_line_coding_t line_coding;
} acmh_handle_t;

/* @global */
static acmh_handle_t *acm_list = NULL;
static acmh_handle_t handle_pool[CDCH_ACM_HANDLE_POOL_SIZE] = {0};

/* @function declarations */
static bool acm_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length);
static bool acm_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num);
static void acm_drv_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);
static void acm_drv_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);

static const usbh_drv_info_t acm_drv_info = {
    .vid = 0x0000,
    .pid = 0x0000,
    .dev_class = 0x00,
    .dev_subclass = 0x00,
    .dev_protocol = 0x00,
    .itf_class = USB_CLASS_CDC,
    .itf_subclass = CDC_SUBCLASS_ABSTRACT_CONTROL_MODEL,
    .itf_protocol = 0x00,
    .match_bitmap = (USBH_DRV_MATCH_ITF_CLASS | USBH_DRV_MATCH_ITF_SUBCLASS),
    .bind = acm_drv_bind_cb,
    .unbind = acm_drv_unbind_cb,
};

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    acmh_handle_t *acm = (acmh_handle_t *)handle;
    if (!rst)
    {
        USB_LOGE("Handle %p: Enumeration control transfer failed at stage %d", acm, acm->enum_stage);
        acm->enum_stage = ENUM_STAGE_ENUM_FAILED;
        return;
    }

    switch (acm->enum_stage)
    {
    case ENUM_STAGE_GET_LINE_CODING:
    {
        acm->enum_stage = ENUM_STAGE_SET_CONTROL_LINE_STATE;
        USB_LOGI("Handle %p: GET_LINE_CODING: Bit_Rate %u, Stop_Bits %d, Parity %d, Data_Bits %d", acm,
                 acm->line_coding.bit_rate, acm->line_coding.stop_bits, acm->line_coding.parity,
                 acm->line_coding.data_bits);

        usb_setup_t *request = &acm->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = CDC_REQ_SET_CONTROL_LINE_STATE;
        request->wValue = 0;
        request->wIndex = acm->ctrl_itf->bInterfaceNumber;
        request->wLength = 0;
        usbh_ctrl_xfer(acm->dev, acm, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_SET_CONTROL_LINE_STATE:
    {
        if (!usbh_endp_open(acm->dev, acm, acm->notify_ep, NULL))
        {
            acm->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle %p: Failed to open notify endpoint", acm);
            break;
        }
        if (!usbh_endp_open(acm->dev, acm, acm->in_ep, acm_drv_read_cb))
        {
            acm->enum_stage = ENUM_STAGE_ENUM_FAILED;
            usbh_endp_close(acm->dev, acm->notify_ep->bEndpointAddress);
            USB_LOGE("Handle %p: Failed to open IN endpoint", acm);
            break;
        }
        if (!usbh_endp_open(acm->dev, acm, acm->out_ep, acm_drv_write_cb))
        {
            acm->enum_stage = ENUM_STAGE_ENUM_FAILED;
            usbh_endp_close(acm->dev, acm->notify_ep->bEndpointAddress);
            usbh_endp_close(acm->dev, acm->in_ep->bEndpointAddress);
            USB_LOGE("Handle %p: Failed to open OUT endpoint", acm);
            break;
        }
        acm->enum_stage = ENUM_STAGE_END;
        USB_LOGI("Handle %p: Enumeration completed", acm);
    }
    }
}

static void acm_enum_task(acmh_handle_t *acm)
{
    switch (acm->enum_stage)
    {
    case ENUM_STAGE_IDLE:
    {
        if (usbh_is_ctrl_xfering(acm->dev)) return;
        acm->enum_stage = ENUM_STAGE_GET_LINE_CODING;
        usb_setup_t *request = &acm->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = CDC_REQ_GET_LINE_CODING;
        request->wValue = 0;
        request->wIndex = acm->ctrl_itf->bInterfaceNumber;
        request->wLength = sizeof(acm->line_coding);
        usbh_ctrl_xfer(acm->dev, acm, request, &acm->line_coding, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_ENUM_FAILED:
        acm->enum_stage = ENUM_STAGE_END;
        acm_drv_unbind_cb(acm->dev, acm->ctrl_itf->bInterfaceNumber);
        break;
    }
}

static void run_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    acmh_handle_t *acm = (acmh_handle_t *)handle;
    if (!acm->opening) return;
    USB_LOGI("Handle %p: Successfully opened Baud rate: %u", acm, acm->line_coding.bit_rate);
    acm->opening = false;
    acm->is_open = rst;
    if (acm->open_cb)
    {
        acm->open_cb(acm, rst);
    }
}

static void acm_run_task(acmh_handle_t *acm)
{
    if (usbh_is_ctrl_xfering(acm->dev)) return;
    if (acm->opening)
    {
        usb_setup_t *request = &acm->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = CDC_REQ_SET_LINE_CODING;
        request->wValue = 0;
        request->wIndex = acm->ctrl_itf->bInterfaceNumber;
        request->wLength = sizeof(acm->line_coding);
        usbh_ctrl_xfer(acm->dev, acm, request, &acm->line_coding, CTRL_XFER_TIMEOUT, run_ctrl_xfer_cb);
    }

    if (!acm->is_open) return;
    if (acm->line_state_changed)
    {
        acm->line_state_changed = false;
        usb_setup_t *request = &acm->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = CDC_REQ_SET_CONTROL_LINE_STATE;
        request->wValue = acm->line_state;
        request->wIndex = acm->ctrl_itf->bInterfaceNumber;
        request->wLength = 0;
        usbh_ctrl_xfer(acm->dev, acm, request, NULL, CTRL_XFER_TIMEOUT, NULL);
    }
    else if (acm->break_changed)
    {
        acm->break_changed = false;
        usb_setup_t *request = &acm->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = CDC_REQ_SEND_BREAK;
        request->wValue = acm->duration;
        request->wIndex = acm->ctrl_itf->bInterfaceNumber;
        request->wLength = 0;
        usbh_ctrl_xfer(acm->dev, acm, request, NULL, CTRL_XFER_TIMEOUT, NULL);
    }
}

void cdch_acm_drv_task(void)
{
    acmh_handle_t *acm = acm_list;
    while (acm)
    {
        acmh_handle_t *next = acm->next;
        if (!acm->dev->suspended)
        {
            if (acm->enum_stage < ENUM_STAGE_END)
            {
                acm_enum_task(acm);
            }
            else
            {
                acm_run_task(acm);
            }
        }
        acm = next;
    }
}

bool cdch_acm_drv_open(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_register_driver(h, &acm_drv_info);
}

bool cdch_acm_drv_close(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_unregister_driver(h, &acm_drv_info);
}

size_t cdch_acm_get_count(void)
{
    size_t count = 0;
    acmh_handle_t *acm = acm_list;
    while (acm)
    {
        if (acm->enum_stage >= ENUM_STAGE_END) count++;
        acm = acm->next;
    }
    return count;
}

acmh_handle_t *cdch_acm_get_handle(size_t index)
{
    size_t i = 0;
    acmh_handle_t *acm = acm_list;
    while (acm)
    {
        if (acm->enum_stage >= ENUM_STAGE_END)
        {
            if (i == index) return acm;
            i++;
        }
        acm = acm->next;
    }
    return NULL;
}

void cdch_acm_open(acmh_handle_t *acm, const cdc_line_coding_t *line_coding, cdch_acm_open_cb cb)
{
    if (!acm || !line_coding)
    {
        if (cb) cb(acm, false);
        return;
    }
    acm->open_cb = cb;
    acm->opening = true;
    memcpy(&acm->line_coding, line_coding, sizeof(cdc_line_coding_t));
}

void cdch_acm_close(acmh_handle_t *acm)
{
    if (!acm) return;
    acm->is_open = false;
    acm->opening = false;
}

bool cdch_acm_is_open(acmh_handle_t *acm)
{
    if (!acm) return false;
    return acm->is_open;
}

bool cdch_acm_read(acmh_handle_t *acm, void *buf, size_t length, uint32_t timeout, cdch_acm_read_cb cb)
{
    if (!acm || !buf || length == 0 || !acm->is_open) return false;
    if (usbh_is_endp_xfering(acm->dev, acm->in_ep->bEndpointAddress)) return false;
    acm->read_cb = cb;
    usbh_endp_read(acm->dev, acm->in_ep->bEndpointAddress, buf, length, timeout);
    return true;
}

bool cdch_acm_write(acmh_handle_t *acm, const void *buf, size_t length, uint32_t timeout, cdch_acm_write_cb cb)
{
    if (!acm || !buf || length == 0 || !acm->is_open) return false;
    if (usbh_is_endp_xfering(acm->dev, acm->out_ep->bEndpointAddress)) return false;
    acm->write_cb = cb;
    usbh_endp_write(acm->dev, acm->out_ep->bEndpointAddress, buf, length, timeout);
    return true;
}

void cdch_acm_set_dtr(acmh_handle_t *acm, bool dtr)
{
    if (!acm || !acm->is_open) return;
    USB_LOGI("Handle %p set DTR to %d", acm, dtr);
    acm->line_state_changed = true;
    acm->line_state = (dtr ? acm->line_state | CDC_LINE_STATE_DTR_MASK : acm->line_state & ~CDC_LINE_STATE_DTR_MASK);
}

void cdch_acm_set_rts(acmh_handle_t *acm, bool rts)
{
    if (!acm || !acm->is_open) return;
    USB_LOGI("Handle %p set RTS to %d", acm, rts);
    acm->line_state_changed = true;
    acm->line_state = (rts ? acm->line_state | CDC_LINE_STATE_RTS_MASK : acm->line_state & ~CDC_LINE_STATE_RTS_MASK);
}

void cdch_acm_set_break(acmh_handle_t *acm, bool brk)
{
    if (!acm || !acm->is_open) return;
    USB_LOGI("Handle %p set BREAK to %d", acm, brk);
    acm->break_changed = true;
    acm->duration = brk ? 0xFFFF : 0;
}

void cdch_acm_send_break(acmh_handle_t *acm, uint16_t duration)
{
    if (!acm || !acm->is_open) return;
    USB_LOGI("Handle %p send BREAK with duration %d", acm, duration);
    acm->break_changed = true;
    acm->duration = duration;
}

bool cdch_acm_get_dtr(acmh_handle_t *acm)
{
    if (!acm || !acm->is_open) return false;
    USB_LOGI("Handle %p get DTR state %d", acm, (acm->line_state & CDC_LINE_STATE_DTR_MASK) ? 1 : 0);
    return acm->line_state & CDC_LINE_STATE_DTR_MASK;
}

bool cdch_acm_get_rts(acmh_handle_t *acm)
{
    if (!acm || !acm->is_open) return false;
    USB_LOGI("Handle %p get RTS state %d", acm, (acm->line_state & CDC_LINE_STATE_RTS_MASK) ? 1 : 0);
    return acm->line_state & CDC_LINE_STATE_RTS_MASK;
}

static acmh_handle_t *handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(handle_pool); i++)
    {
        acmh_handle_t *acm = &handle_pool[i];
        if (acm->dev == NULL)
        {
            memset(acm, 0, sizeof(acmh_handle_t));
            return acm;
        }
    }
    return NULL;
}

static bool acm_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length)
{
    if (!dev || dev->speed == USB_SPEED_LOW || !desc || length == 0) return false;

    const usb_desc_interface_t *ctrl_itf = NULL;
    const usb_desc_interface_t *data_itf = NULL;
    const usb_desc_endpoint_t *notify_ep = NULL;
    const usb_desc_endpoint_t *in_ep = NULL;
    const usb_desc_endpoint_t *out_ep = NULL;
    const cdc_desc_abstract_control_management_t *acm_desc = NULL;

    uint8_t curr_itf_type = 0;
    uint8_t endp_count = 0;
    const cdc_desc_call_management_t *cm_desc = NULL;
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
                if (!cm_desc || cm_desc->bDataInterface != curr_itf->bInterfaceNumber) return false;
                if (!union_desc || union_desc->bSubordinateInterface[0] != curr_itf->bInterfaceNumber) return false;
                if (curr_itf->bNumEndpoints > 2) return false;
                if (curr_itf->bAlternateSetting != 0) break;
                data_itf = curr_itf;
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
            else if (class_type == CDC_DESC_CALL_MANAGEMENT)
            {
                cm_desc = (cdc_desc_call_management_t *)curr;
                if (cm_desc->bLength < sizeof(cdc_desc_call_management_t)) return false;
            }
            else if (class_type == CDC_DESC_ABSTRACT_CONTROL_MANAGEMENT)
            {
                acm_desc = (cdc_desc_abstract_control_management_t *)curr;
                if (acm_desc->bLength < sizeof(cdc_desc_abstract_control_management_t)) return false;
            }
            break;
        }
        }

        curr += len;
    }

    if (!ctrl_itf || !data_itf || !notify_ep || !in_ep || !out_ep || !acm_desc) return false;

    /* Allocate and initialize a new cdc acm handle */
    acmh_handle_t *acm = handle_alloc();
    if (!acm)
    {
        USB_LOGE("Handle %p: Failed to allocate cdc acm handle", dev);
        return false;
    }

    acm->dev = dev;
    acm->ctrl_itf = ctrl_itf;
    acm->data_itf = data_itf;
    acm->notify_ep = notify_ep;
    acm->in_ep = in_ep;
    acm->out_ep = out_ep;
    acm->acm_desc = acm_desc;
    usbh_list_append((void **)&acm_list, acm);
    return true;
}

static bool acm_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num)
{
    acmh_handle_t *acm = acm_list;
    while (acm)
    {
        acmh_handle_t *next = acm->next;
        if (acm->dev == dev && (itf_num == USBH_DRV_ALL_INTERFACE || acm->ctrl_itf->bInterfaceNumber == itf_num))
        {
            usbh_endp_close(acm->dev, acm->notify_ep->bEndpointAddress);
            usbh_endp_close(acm->dev, acm->in_ep->bEndpointAddress);
            usbh_endp_close(acm->dev, acm->out_ep->bEndpointAddress);
            usbh_list_remove((void **)&acm_list, acm);
            memset(acm, 0, sizeof(acmh_handle_t));
        }
        acm = next;
    }
    return true;
}

static void acm_drv_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    acmh_handle_t *acm = (acmh_handle_t *)handle;
    if (rst && acm->is_open)
    {
        USB_LOGI("Handle %p: Read %u bytes", acm, len);
        if (acm->read_cb)
        {
            acm->read_cb(acm, buf, len);
        }
    }
}

static void acm_drv_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    acmh_handle_t *acm = (acmh_handle_t *)handle;
    if (rst && acm->is_open)
    {
        USB_LOGI("Handle %p: Write %u bytes", acm, len);
        if (acm->write_cb)
        {
            acm->write_cb(acm, buf, len);
        }
    }
}

#endif // USB_CLASS_CDCH_ACM_DRIVER_EN
