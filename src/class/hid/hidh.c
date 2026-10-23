/********************************** (C) COPYRIGHT *******************************
* File Name          : hidh.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB HID host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "HIDH"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"
#include "class/hid/hidh_private.h"

#ifdef USB_CLASS_HIDH_DRIVER_EN

/* @define */
#ifndef HIDH_HANDLE_POOL_SIZE
#define HIDH_HANDLE_POOL_SIZE 1
#endif

#ifndef HIDH_REPORT_ITEM_POOL_SIZE
#define HIDH_REPORT_ITEM_POOL_SIZE 32
#endif

#ifndef HIDH_MAX_INSTANCE_COUNT
#define HIDH_MAX_INSTANCE_COUNT 5
#endif

#ifndef HIDH_REPORT_DESC_BUF_SIZE
#define HIDH_REPORT_DESC_BUF_SIZE 512
#endif

#ifndef HIDH_GET_REPORT_BUF_SIZE
#define HIDH_GET_REPORT_BUF_SIZE 128
#endif

#ifndef HIDH_SET_REPORT_BUF_SIZE
#define HIDH_SET_REPORT_BUF_SIZE 128
#endif

#define CTRL_XFER_TIMEOUT          100
#define SUPPORTED_PROTOCOL_VERSION 0x0111
#define STACK_MAX_DEPTH            8

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
typedef struct
{
    uint8_t report_id;
    uint16_t usage_page;

    uint32_t size;
    uint32_t count;

    int32_t logical_min;
    int32_t logical_max;
    int32_t physical_min;
    int32_t physical_max;
    int32_t unit_exponent;
    uint32_t unit;
} report_info_t;

typedef struct report_item
{
    struct report_item *next;
    bool used;
    uint16_t flags;
    uint16_t local_report_size;
    const uint8_t *local_report_buf;
    report_info_t global_info;
} report_item_t;

typedef struct
{
    uint8_t stack_depth;
    uint16_t top_usage;
    uint16_t top_usage_page;
    hidh_instance_t *instance;
    const uint8_t *last_local_buf;
    const uint8_t *local_report_buf;
    report_info_t global_info;
    report_info_t global_info_stack[STACK_MAX_DEPTH];
} parse_ctx_t;

typedef struct hidh_handle
{
    struct hidh_handle *next;
    usbh_device_t *dev;

    /* HID interface information */
    const usb_desc_interface_t *itf_desc;
    const usb_desc_hid_t *hid_desc;
    const usb_desc_endpoint_t *in_ep;
    const usb_desc_endpoint_t *out_ep;
    __attribute__((aligned(4))) uint8_t report_desc_buf[HIDH_REPORT_DESC_BUF_SIZE];

    /* Run-time context */
    uint8_t enum_stage;
    uint8_t instance_count;
    hidh_instance_t *instances[HIDH_MAX_INSTANCE_COUNT];
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
    __attribute__((aligned(4))) uint8_t get_report_buf[HIDH_GET_REPORT_BUF_SIZE];
    __attribute__((aligned(4))) uint8_t set_report_buf[HIDH_SET_REPORT_BUF_SIZE];
} hidh_handle_t;

/* @global */
static hidh_handle_t *hid_list = NULL;
static hidh_handle_t handle_pool[HIDH_HANDLE_POOL_SIZE] = {0};
static report_item_t report_item_pool[HIDH_REPORT_ITEM_POOL_SIZE] = {0};

/* @function declarations */
static bool hid_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length);
static bool hid_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num);
static void hid_drv_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);

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

static report_item_t *report_item_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(report_item_pool); i++)
    {
        report_item_t *item = &report_item_pool[i];
        if (!item->used)
        {
            memset(item, 0, sizeof(report_item_t));
            item->used = true;
            return item;
        }
    }
    return NULL;
}

static hidh_instance_t *hidh_instance_alloc(hidh_handle_t *hid, uint16_t usage_page, uint16_t usage)
{
    if (usage_page == HID_USAGE_PAGE_DESKTOP)
    {
        switch (usage)
        {
#ifdef USB_CLASS_HIDH_KEYBOARD_EN
        case HID_DESKTOP_USAGE_KEYBOARD:
            return hidh_keyboard_create_instance(hid);
#endif

#ifdef USB_CLASS_HIDH_MOUSE_EN
        case HID_DESKTOP_USAGE_MOUSE:
            return hidh_mouse_create_instance(hid);
#endif
        }
    }

    return NULL;
}

static bool report_main_item_parse(hidh_handle_t *hid, parse_ctx_t *ctx, uint8_t tag, uint8_t size, uint32_t value)
{
    switch (tag)
    {
    case HID_MAIN_ITEM_INPUT:
    case HID_MAIN_ITEM_OUTPUT:
    case HID_MAIN_ITEM_FEATURE:
    {
        if (ctx->instance)
        {
            report_item_t *new_item = report_item_alloc();
            if (!new_item) return false;
            new_item->flags = value;
            new_item->local_report_buf = ctx->last_local_buf;
            new_item->local_report_size = ctx->local_report_buf - ctx->last_local_buf;
            memcpy(&new_item->global_info, &ctx->global_info, sizeof(report_info_t));
            switch (tag)
            {
            case HID_MAIN_ITEM_INPUT:
                usbh_list_append(&ctx->instance->input_items, new_item);
                break;

            case HID_MAIN_ITEM_OUTPUT:
                usbh_list_append(&ctx->instance->output_items, new_item);
                break;

            case HID_MAIN_ITEM_FEATURE:
                usbh_list_append(&ctx->instance->feature_items, new_item);
                break;
            }
        }
        break;
    }

    case HID_MAIN_ITEM_COLLECTION:
        if (ctx->stack_depth == 0 && value == HID_COLLECTION_APPLICATION)
        {
            if (hid->instance_count >= USB_ARRAY_SIZE(hid->instances)) return false;
            ctx->instance = hidh_instance_alloc(hid, ctx->top_usage_page, ctx->top_usage);
            if (ctx->instance)
            {
                hid->instances[hid->instance_count++] = ctx->instance;
            }
        }
        if (ctx->stack_depth >= USB_ARRAY_SIZE(ctx->global_info_stack)) return false;
        memcpy(&ctx->global_info_stack[ctx->stack_depth++], &ctx->global_info, sizeof(report_info_t));
        break;

    case HID_MAIN_ITEM_END_COLLECTION:
        if (ctx->stack_depth == 0) return false;
        memcpy(&ctx->global_info, &ctx->global_info_stack[--ctx->stack_depth], sizeof(report_info_t));
        break;

    default:
        return false;
    }

    /* Reset local information */
    ctx->last_local_buf = ctx->local_report_buf;
    return true;
}

static bool report_global_item_parse(hidh_handle_t *hid, parse_ctx_t *ctx, uint8_t tag, uint8_t size, uint32_t value)
{
    int32_t signed_value = 0;
    if (size == 1)
        signed_value = (int8_t)value;
    else if (size == 2)
        signed_value = (int16_t)value;
    else
        signed_value = (int32_t)value;

    report_info_t *global_info = &ctx->global_info;
    switch (tag)
    {
    case HID_GLOBAL_ITEM_USAGE_PAGE:
        global_info->usage_page = value;
        if (ctx->stack_depth == 0)
        {
            ctx->top_usage_page = value;
        }
        break;

    case HID_GLOBAL_ITEM_LOGICAL_MINIMUM:
        global_info->logical_min = signed_value;
        break;

    case HID_GLOBAL_ITEM_LOGICAL_MAXIMUM:
        global_info->logical_max = signed_value;
        break;

    case HID_GLOBAL_ITEM_PHYSICAL_MINIMUM:
        global_info->physical_min = signed_value;
        break;

    case HID_GLOBAL_ITEM_PHYSICAL_MAXIMUM:
        global_info->physical_max = signed_value;
        break;

    case HID_GLOBAL_ITEM_UNIT_EXPONENT:
        global_info->unit_exponent = signed_value;
        break;

    case HID_GLOBAL_ITEM_UNIT:
        global_info->unit = value;
        break;

    case HID_GLOBAL_ITEM_REPORT_SIZE:
        if (value > 32) return false;
        global_info->size = value;
        break;

    case HID_GLOBAL_ITEM_REPORT_ID:
        if (value == 0) return false;
        global_info->report_id = value;
        if (ctx->instance)
        {
            ctx->instance->report_id = value;
        }
        break;

    case HID_GLOBAL_ITEM_REPORT_COUNT:
        if (value > 8192) return false;
        global_info->count = value;
        break;

    case HID_GLOBAL_ITEM_PUSH:
        if (ctx->stack_depth >= USB_ARRAY_SIZE(ctx->global_info_stack)) return false;
        memcpy(&ctx->global_info_stack[ctx->stack_depth++], &ctx->global_info, sizeof(report_info_t));
        break;

    case HID_GLOBAL_ITEM_POP:
        if (ctx->stack_depth == 0) return false;
        memcpy(&ctx->global_info, &ctx->global_info_stack[--ctx->stack_depth], sizeof(report_info_t));
        break;

    default:
        return false;
    }
    return true;
}

static bool report_local_item_parse(hidh_handle_t *hid, parse_ctx_t *ctx, uint8_t tag, uint8_t size, uint32_t value)
{
    switch (tag)
    {
    case HID_LOCAL_ITEM_USAGE:
        if (ctx->stack_depth == 0)
        {
            ctx->top_usage = value & 0xFFFF;
            if (size >= 4)
            {
                ctx->top_usage_page = (value >> 16) & 0xFFFF;
            }
        }
        break;

    case HID_LOCAL_ITEM_USAGE_MINIMUM:
    case HID_LOCAL_ITEM_USAGE_MAXIMUM:
    case HID_LOCAL_ITEM_DESIGNATOR_INDEX:
    case HID_LOCAL_ITEM_DESIGNATOR_MINIMUM:
    case HID_LOCAL_ITEM_DESIGNATOR_MAXIMUM:
    case HID_LOCAL_ITEM_STRING_INDEX:
    case HID_LOCAL_ITEM_STRING_MINIMUM:
    case HID_LOCAL_ITEM_STRING_MAXIMUM:
    case HID_LOCAL_ITEM_DELIMITER:
        break;

    default:
        return false;
    }
    return true;
}

static bool hid_report_desc_parse(hidh_handle_t *hid, const void *buf, size_t len)
{
    if (!hid || !buf || len == 0) return false;

    parse_ctx_t ctx = {0};
    ctx.last_local_buf = (const uint8_t *)buf;

    const uint8_t *curr = (const uint8_t *)buf;
    const uint8_t *end = (const uint8_t *)buf + len;

    while (curr < end)
    {
        /* Not supported long item */
        if (*curr == HID_LONG_ITEM_VALUE) return false;

        uint8_t item_type = HID_GET_ITEM_TYPE(*curr);
        uint8_t item_tag = HID_GET_ITEM_TAG(*curr);
        uint8_t item_size = HID_GET_ITEM_SIZE(*curr) == 3 ? 4 : HID_GET_ITEM_SIZE(*curr);
        if (curr + item_size >= end) return false;

        /* Convert the item data to a 32-bit value */
        uint32_t value = 0;
        for (uint8_t i = 0; i < item_size; i++)
        {
            value |= ((uint32_t)curr[i + 1] << (8 * i));
        }

        switch (item_type)
        {
        case HID_ITEM_TYPE_MAIN:
            ctx.local_report_buf = curr + item_size + 1;
            if (!report_main_item_parse(hid, &ctx, item_tag, item_size, value)) return false;
            break;

        case HID_ITEM_TYPE_GLOBAL:
            if (!report_global_item_parse(hid, &ctx, item_tag, item_size, value)) return false;
            break;

        case HID_ITEM_TYPE_LOCAL:
            if (!report_local_item_parse(hid, &ctx, item_tag, item_size, value)) return false;
            break;

        default:
            return false;
        }

        curr += item_size + 1;
    }

    if (ctx.stack_depth != 0) return false;
    if (hid->instance_count == 0) return false;
    return true;
}

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    hidh_handle_t *hid = (hidh_handle_t *)handle;
    if (!rst && hid->enum_stage != ENUM_STAGE_SET_IDLE)
    {
        USB_LOGE("Handle %p: Enumeration control transfer failed at stage %d", hid, hid->enum_stage);
        hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
        return;
    }

    switch (hid->enum_stage)
    {
    case ENUM_STAGE_SET_IDLE:
    {
        hid->enum_stage = ENUM_STAGE_GET_DESC;
        USB_LOGI("Handle %p: SET_IDLE", hid);
        for (size_t i = 0; i < hid->hid_desc->bNumDescriptors; i++)
        {
            if (hid->hid_desc->descriptor[i].bReportType == HID_DESC_REPORT)
            {
                if (hid->hid_desc->descriptor[i].wReportLength > sizeof(hid->report_desc_buf)) break;

                /* Get only the first report descriptor */
                usb_setup_t *request = &hid->ctrl_setup;
                request->bmRequestType = USB_SET_REQ(USB_DIR_IN, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
                request->bRequest = USB_REQ_GET_DESCRIPTOR;
                request->wValue = hid->hid_desc->descriptor[i].bReportType << 8;
                request->wIndex = hid->itf_desc->bInterfaceNumber;
                request->wLength = hid->hid_desc->descriptor[i].wReportLength;
                usbh_ctrl_xfer(hid->dev, hid, request, &hid->report_desc_buf, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
                return;
            }
        }
        hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
        USB_LOGE("Handle %p: No report descriptor found", hid);
        break;
    }
    case ENUM_STAGE_GET_DESC:
        if (length != setup->wLength)
        {
            hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle %p: Get descriptor failed: expected length %d, got %d", hid, setup->wLength, length);
            break;
        }

        if (!hid_report_desc_parse(hid, &hid->report_desc_buf, length))
        {
            hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle %p: Failed to parse HID report descriptor", hid);
            break;
        }

        if (hid->in_ep && !usbh_endp_open(hid->dev, hid, hid->in_ep, hid_drv_read_cb))
        {
            hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle %p: Failed to open IN endpoint", hid);
            break;
        }

        if (hid->out_ep && !usbh_endp_open(hid->dev, hid, hid->out_ep, NULL))
        {
            hid->enum_stage = ENUM_STAGE_ENUM_FAILED;
            USB_LOGE("Handle %p: Failed to open OUT endpoint", hid);
            if (hid->in_ep) usbh_endp_close(hid->dev, hid->in_ep->bEndpointAddress);
            break;
        }

        hid->enum_stage = ENUM_STAGE_END;
        USB_LOGI("Handle %p: Enumeration completed", hid);
        break;
    }
}

static void hid_enum_task(hidh_handle_t *hid)
{
    switch (hid->enum_stage)
    {
    case ENUM_STAGE_IDLE:
    {
        if (usbh_is_ctrl_xfering(hid->dev)) return;
        hid->enum_stage = ENUM_STAGE_SET_IDLE;
        usb_setup_t *request = &hid->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = HID_REQ_SET_IDLE;
        request->wValue = 0;
        request->wIndex = hid->itf_desc->bInterfaceNumber;
        request->wLength = 0;
        usbh_ctrl_xfer(hid->dev, hid, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_ENUM_FAILED:
        hid->enum_stage = ENUM_STAGE_END;
        hid_drv_unbind_cb(hid->dev, hid->itf_desc->bInterfaceNumber);
        break;
    }
}

static void hid_run_task(hidh_handle_t *hid)
{
    /* Get report from IN endpoint */
    if (hid->in_ep)
    {
        usbh_endp_read(hid->dev, hid->in_ep->bEndpointAddress, hid->get_report_buf, sizeof(hid->get_report_buf), 0);
    }
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

#ifdef USB_CLASS_HIDH_KEYBOARD_EN
    hidh_keyboard_task();
#endif

#ifdef USB_CLASS_HIDH_MOUSE_EN
    hidh_mouse_task();
#endif
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

static uint16_t *variable_usage_parse(const report_item_t *item)
{
    static uint16_t usage_parse_list[256] = {0};
    if (item->global_info.count > USB_ARRAY_SIZE(usage_parse_list)) return NULL;

    uint16_t usage_min = 0xFFFF;
    uint16_t usage_max = 0;
    uint32_t usage_offset = 0;

    const uint8_t *curr = (const uint8_t *)item->local_report_buf;
    const uint8_t *end = (const uint8_t *)item->local_report_buf + item->local_report_size;
    while (curr < end)
    {
        uint8_t item_type = HID_GET_ITEM_TYPE(*curr);
        uint8_t item_tag = HID_GET_ITEM_TAG(*curr);
        uint8_t item_size = HID_GET_ITEM_SIZE(*curr) == 3 ? 4 : HID_GET_ITEM_SIZE(*curr);

        if (item_type != HID_ITEM_TYPE_LOCAL)
        {
            curr += item_size + 1;
            continue;
        }

        uint32_t value = 0;
        for (uint8_t i = 0; i < item_size; i++)
        {
            value |= ((uint32_t)curr[i + 1] << (8 * i));
        }

        switch (item_tag)
        {
        case HID_LOCAL_ITEM_USAGE:
            if (usage_offset >= USB_ARRAY_SIZE(usage_parse_list)) break;
            usage_parse_list[usage_offset++] = (uint16_t)value;
            break;

        case HID_LOCAL_ITEM_USAGE_MINIMUM:
            usage_min = (uint16_t)value;
            break;

        case HID_LOCAL_ITEM_USAGE_MAXIMUM:
            usage_max = (uint16_t)value;
            if (usage_max < usage_min) break;
            for (uint16_t usage = usage_min; usage <= usage_max; usage++)
            {
                if (usage_offset >= USB_ARRAY_SIZE(usage_parse_list)) break;
                usage_parse_list[usage_offset++] = usage;
            }
            break;
        }
        curr += item_size + 1;
    }

    uint16_t last_usage = usage_offset ? usage_parse_list[usage_offset - 1] : 0;
    for (uint32_t i = usage_offset; i < item->global_info.count; i++)
    {
        if (i >= USB_ARRAY_SIZE(usage_parse_list)) break;
        usage_parse_list[i] = last_usage;
    }

    return usage_parse_list;
}

static bool array_usage_parse(const report_item_t *item, uint16_t *usage_min, uint16_t *usage_max)
{
    uint16_t min = 0xFFFF;
    uint16_t max = 0;

    const uint8_t *curr = (const uint8_t *)item->local_report_buf;
    const uint8_t *end = (const uint8_t *)item->local_report_buf + item->local_report_size;
    while (curr < end)
    {
        uint8_t item_type = HID_GET_ITEM_TYPE(*curr);
        uint8_t item_tag = HID_GET_ITEM_TAG(*curr);
        uint8_t item_size = HID_GET_ITEM_SIZE(*curr) == 3 ? 4 : HID_GET_ITEM_SIZE(*curr);

        if (item_type != HID_ITEM_TYPE_LOCAL)
        {
            curr += item_size + 1;
            continue;
        }

        uint32_t value = 0;
        for (uint8_t i = 0; i < item_size; i++)
        {
            value |= ((uint32_t)curr[i + 1] << (8 * i));
        }

        switch (item_tag)
        {
        case HID_LOCAL_ITEM_USAGE:
            min = USB_MIN(min, (uint16_t)value);
            max = USB_MAX(max, (uint16_t)value);
            break;

        case HID_LOCAL_ITEM_USAGE_MINIMUM:
            min = (uint16_t)value;
            break;

        case HID_LOCAL_ITEM_USAGE_MAXIMUM:
            max = (uint16_t)value;
            break;
        }
        curr += item_size + 1;
    }

    if (min > max)
    {
        return false;
    }

    *usage_min = min;
    *usage_max = max;
    return true;
}

static void set_field(uint8_t *data, uint32_t offset, uint8_t size, uint32_t value)
{
    for (uint8_t i = 0; i < size && i < 32; i++)
    {
        uint32_t bit = offset + i;
        if (value & (1U << i))
        {
            data[bit >> 3] |= (1U << (bit & 0x07));
        }
        else
        {
            data[bit >> 3] &= ~(1U << (bit & 0x07));
        }
    }
}

bool hidh_set_report(hidh_handle_t *hid, hidh_instance_t *instance)
{
    if (!hid || !instance || !instance->set_report_cb || !instance->output_items) return false;

    if (hid->out_ep)
    {
        if (usbh_is_endp_xfering(hid->dev, hid->out_ep->bEndpointAddress)) return false;
    }
    else
    {
        if (usbh_is_ctrl_xfering(hid->dev)) return false;
    }

    uint8_t *data = hid->set_report_buf;
    uint32_t bit_offset = 0;

    if (instance->report_id)
    {
        hid->set_report_buf[0] = instance->report_id;
        bit_offset = 8;
    }

    report_item_t *item = (report_item_t *)instance->output_items;
    while (item)
    {
        if ((bit_offset + item->global_info.size * item->global_info.count + 7) >> 3 > sizeof(hid->set_report_buf))
            break;

        if (item->flags & HID_ITEM_INPUT_FLAG_CONSTANT)
        {
            bit_offset += item->global_info.size * item->global_info.count;
            item = item->next;
            continue;
        }

        if (item->flags & HID_ITEM_INPUT_FLAG_VARIABLE)
        {
            uint16_t *usage_list = variable_usage_parse(item);
            if (!usage_list) break;

            for (uint32_t i = 0; i < item->global_info.count; i++)
            {
                uint32_t curr_offset = bit_offset + i * item->global_info.size;
                uint16_t usage = usage_list[i];
                uint32_t value = instance->set_report_cb(instance, item->global_info.usage_page, usage);
                set_field(data, curr_offset, item->global_info.size, value);
            }
        }

        bit_offset += item->global_info.size * item->global_info.count;
        item = item->next;
    }

    if (hid->out_ep)
    {
        usbh_endp_write(hid->dev, hid->out_ep->bEndpointAddress, hid->set_report_buf, bit_offset + 7 >> 3, 0);
    }
    else
    {
        usb_setup_t *request = &hid->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_INTERFACE);
        request->bRequest = HID_REQ_SET_REPORT;
        request->wValue = (HID_REPORT_TYPE_OUTPUT << 8) | instance->report_id;
        request->wIndex = hid->itf_desc->bInterfaceNumber;
        request->wLength = bit_offset + 7 >> 3;
        usbh_ctrl_xfer(hid->dev, hid, request, hid->set_report_buf, CTRL_XFER_TIMEOUT, NULL);
    }

    return true;
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

static void hid_handle_free(hidh_handle_t *hid)
{
    for (uint16_t i = 0; i < hid->instance_count; i++)
    {
        hidh_instance_t *instance = hid->instances[i];
        void **item_list[3] = {&instance->input_items, &instance->output_items, &instance->feature_items};
        for (uint8_t j = 0; j < USB_ARRAY_SIZE(item_list); j++)
        {
            report_item_t *item = (report_item_t *)(*item_list[j]);
            while (item)
            {
                item->used = false;
                item = item->next;
            }
        }
        if (instance->delete_cb)
        {
            instance->delete_cb(instance);
        }
    }
}

static bool hid_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t length)
{
    if (!dev || !desc || length == 0) return false;

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
            if (endp_type != USB_ENDP_TYPE_INTR) return false;
            if (!endp_count) break;
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
            hid_handle_free(hid);
            memset(hid, 0, sizeof(hidh_handle_t));
        }
        hid = next;
    }
    return true;
}

static int32_t get_field(const uint8_t *data, uint32_t offset, uint8_t size, bool is_signed)
{
    uint32_t value = 0;
    for (uint8_t i = 0; i < size && i < 32; i++)
    {
        uint32_t bit = offset + i;
        value |= data[bit >> 3] & (1U << (bit & 0x07)) ? 1U << i : 0;
    }

    if (is_signed && 0 < size && size < 32)
    {
        uint32_t sign_bit = 1U << (size - 1);
        if (value & sign_bit)
        {
            value |= ~((1U << size) - 1);
        }
    }
    return (int32_t)value;
}

static void get_report_parse(hidh_instance_t *instance, const uint8_t *data, size_t len)
{
    if (!len || !instance->get_report_cb) return;

    if (instance->start_parse_cb)
    {
        instance->start_parse_cb(instance);
    }

    uint32_t bit_offset = 0;
    report_item_t *item = (report_item_t *)instance->input_items;
    while (item)
    {
        if ((bit_offset + item->global_info.size * item->global_info.count + 7) >> 3 > len) break;

        if (item->flags & HID_ITEM_INPUT_FLAG_CONSTANT)
        {
            bit_offset += item->global_info.size * item->global_info.count;
            item = item->next;
            continue;
        }

        bool is_signed = item->global_info.logical_min < 0;
        if (item->flags & HID_ITEM_INPUT_FLAG_VARIABLE)
        {
            uint16_t *usage_list = variable_usage_parse(item);
            if (!usage_list) return;

            for (uint32_t i = 0; i < item->global_info.count; i++)
            {
                uint32_t curr_offset = bit_offset + i * item->global_info.size;
                int32_t value = get_field(data, curr_offset, item->global_info.size, is_signed);
                uint16_t usage = usage_list[i];
                instance->get_report_cb(instance, item->global_info.usage_page, usage, value);
            }
        }
        else
        {
            uint16_t usage_min = 0;
            uint16_t usage_max = 0;
            if (!array_usage_parse(item, &usage_min, &usage_max)) return;

            for (uint32_t i = 0; i < item->global_info.count; i++)
            {
                uint32_t curr_offset = bit_offset + i * item->global_info.size;
                uint16_t usage = usage_min + get_field(data, curr_offset, item->global_info.size, is_signed);
                usage = USB_MIN(usage, usage_max);
                instance->get_report_cb(instance, item->global_info.usage_page, usage, 1);
            }
        }

        bit_offset += item->global_info.size * item->global_info.count;
        item = item->next;
    }
}

static void hid_drv_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    if (!rst || !len) return;

    hidh_handle_t *hid = (hidh_handle_t *)handle;

    if (!hid->instances[0]) return;
    if (hid->instances[0]->report_id == 0)
    {
        get_report_parse(hid->instances[0], &hid->get_report_buf[0], len);
    }
    else
    {
        for (uint16_t i = 0; i < hid->instance_count; i++)
        {
            hidh_instance_t *instance = hid->instances[i];
            if (instance->report_id == hid->get_report_buf[0])
            {
                get_report_parse(instance, &hid->get_report_buf[1], len - 1);
                break;
            }
        }
    }
}

#endif // USB_CLASS_HIDH_DRIVER_EN
