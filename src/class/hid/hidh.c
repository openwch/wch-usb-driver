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

#ifndef HIDH_REPORT_ITEM_POOL_SIZE
#define HIDH_REPORT_ITEM_POOL_SIZE 32
#endif

#ifndef HIDH_MAX_REPORT_ID_COUNT
#define HIDH_MAX_REPORT_ID_COUNT 5
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

typedef enum
{
    REPORT_TYPE_INPUT,
    REPORT_TYPE_OUTPUT,
    REPORT_TYPE_FEATURE,
    REPORT_TYPE_COUNT,
} report_type_t;

/* @struct */
typedef struct hid_report_item
{
    struct hid_report_item *next; // Pointer to the next report item in the list
    bool used;                    // Indicates if the report item is used

    uint16_t usage_page;          // Usage page
    uint16_t usage_min;           // Minimum usage
    uint16_t usage_max;           // Maximum usage

    uint32_t size;                // Size of the report item in bits
    uint32_t count;               // Number of report items
    uint32_t offset;              // Bit offset within the report

    int32_t logical_min;          // Logical minimum
    int32_t logical_max;          // Logical maximum
    int32_t physical_min;         // Physical minimum
    int32_t physical_max;         // Physical maximum
    int32_t unit_exponent;        // Unit exponent
    uint32_t unit;                // Unit

    union
    {
        uint16_t flags;

        struct
        {
            uint16_t constant : 1;       // Constant value
            uint16_t variable : 1;       // Variable value
            uint16_t relative : 1;       // Relative value
            uint16_t wrap : 1;           // Wrap value
            uint16_t non_linear : 1;     // Non-linear value
            uint16_t no_preferred : 1;   // No preferred value
            uint16_t null_state : 1;     // Null state value
            uint16_t volatile_ : 1;      // Volatile value
            uint16_t buffered_bytes : 1; // Buffered bytes value
        } io_feat_flags_bits;

        struct
        {
            uint16_t physical : 1;       // Physical collection
            uint16_t application : 1;    // Application collection
            uint16_t logical : 1;        // Logical collection
            uint16_t report : 1;         // Report collection
            uint16_t named_array : 1;    // Named array collection
            uint16_t usage_switch : 1;   // Usage switch collection
            uint16_t usage_modifier : 1; // Usage modifier collection
        } collection_flags_bits;
    };
} hid_report_item_t;

typedef struct
{
    uint8_t report_id;
    hid_report_item_t *item_list[REPORT_TYPE_COUNT];
} hid_item_list_t;

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
    bool has_report_id;
    uint8_t enum_stage;
    hid_item_list_t report_item_list[HIDH_MAX_REPORT_ID_COUNT];
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
    __attribute__((aligned(4))) uint8_t read_buf[1024];
    __attribute__((aligned(4))) uint8_t write_buf[1024];
} hidh_handle_t;

/* @global */
hidh_keyboard_t hidh_keyboard = {0};
hidh_mouse_t hidh_mouse = {0};
hidh_keyboard_changed_cb keyboard_changed_cb = NULL;
hidh_mouse_changed_cb mouse_changed_cb = NULL;
static hidh_handle_t *hid_list = NULL;
static hidh_handle_t handle_pool[HIDH_HANDLE_POOL_SIZE] = {0};
static hid_report_item_t report_item_pool[HIDH_REPORT_ITEM_POOL_SIZE] = {0};
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

static hid_report_item_t *report_item_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(report_item_pool); i++)
    {
        hid_report_item_t *item = &report_item_pool[i];
        if (!item->used)
        {
            memset(item, 0, sizeof(hid_report_item_t));
            item->used = true;
            return item;
        }
    }
    return NULL;
}

static void printf_report_item(const hidh_handle_t *hid)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(hid->report_item_list); i++)
    {
        const hid_item_list_t *item_list = &hid->report_item_list[i];
        if (i && !item_list->report_id) break;

        for (uint8_t j = 0; j < USB_ARRAY_SIZE(item_list->item_list); j++)
        {
            if (!item_list->item_list[j]) continue;
            USB_LOGI("====== Handle %p: Report ID %3d %s Structure ======", hid, item_list->report_id,
                     j == 0   ? "Input"
                     : j == 1 ? "Output"
                              : "Feature");

            const hid_report_item_t *item = item_list->item_list[j];
            while (item)
            {
                USB_LOGI("Usage Page: 0x%04X Usage Min: 0x%04X Usage Max: 0x%04X Size: %u Count: %u Offset: %u",
                         item->usage_page, item->usage_min, item->usage_max, item->size, item->count, item->offset);
                item = item->next;
            }
        }
    }
}

static bool hid_report_desc_parse(hidh_handle_t *hid, const void *buf, size_t len)
{
    if (!hid || !buf || len == 0) return false;

    bool has_item = false;
    uint8_t last_report_id = 0;
    uint8_t report_id_index = 0;
    uint16_t nesting_depth = 0;
    uint8_t stack_depth = 0;
    uint16_t local_usage_page = 0;
    hid_report_item_t item = {0};
    hid_report_item_t stack_item[4];
    hid_report_item_t *item_list[REPORT_TYPE_COUNT] = {0};
    uint32_t report_bit_offset[REPORT_TYPE_COUNT] = {0};

    const uint8_t *curr = (const uint8_t *)buf;
    const uint8_t *end = (const uint8_t *)buf + len;

    item.used = true;
    item.usage_min = 0xFFFF;
    while (curr < end)
    {
        /* Not supported long item */
        if (*curr == HID_LONG_ITEM_VALUE) goto free_items;

        uint8_t item_type = HID_GET_ITEM_TYPE(*curr);
        uint8_t item_tag = HID_GET_ITEM_TAG(*curr);
        uint8_t item_size = HID_GET_ITEM_SIZE(*curr) == 3 ? 4 : HID_GET_ITEM_SIZE(*curr);
        if (curr + item_size >= end) goto free_items;

        /* Convert the item data to a 32-bit value */
        uint32_t value = 0;
        for (uint8_t i = 0; i < item_size; i++)
        {
            value |= ((uint32_t)curr[i + 1] << (8 * i));
        }

        if (item_type == HID_ITEM_TYPE_MAIN)
        {
            switch (item_tag)
            {
            case HID_MAIN_ITEM_INPUT:
            case HID_MAIN_ITEM_OUTPUT:
            case HID_MAIN_ITEM_FEATURE:
            {
                static const uint8_t item_type_to_index[] = {
                    [HID_MAIN_ITEM_INPUT] = REPORT_TYPE_INPUT,
                    [HID_MAIN_ITEM_OUTPUT] = REPORT_TYPE_OUTPUT,
                    [HID_MAIN_ITEM_FEATURE] = REPORT_TYPE_FEATURE,
                };
                hid_report_item_t *new_item = report_item_alloc();
                if (!new_item) goto free_items;
                has_item = true;
                uint8_t index = item_type_to_index[item_tag];
                item.flags = value;
                item.offset = report_bit_offset[index];
                report_bit_offset[index] += item.size * item.count;
                memcpy(new_item, &item, sizeof(hid_report_item_t));
                usbh_list_append((void **)&item_list[index], new_item);
                if (local_usage_page) new_item->usage_page = local_usage_page;
                break;
            }

            case HID_MAIN_ITEM_COLLECTION:
                nesting_depth++;
                break;

            case HID_MAIN_ITEM_END_COLLECTION:
                if (nesting_depth == 0) goto free_items;
                nesting_depth--;
                break;

            default:
                goto free_items;
            }

            /* Reset usage range and local usage page */
            item.usage_min = 0xFFFF;
            item.usage_max = 0;
            local_usage_page = 0;
        }
        else if (item_type == HID_ITEM_TYPE_GLOBAL)
        {
            int32_t signed_value = 0;
            if (item_size == 1)
                signed_value = (int8_t)value;
            else if (item_size == 2)
                signed_value = (int16_t)value;
            else
                signed_value = (int32_t)value;

            switch (item_tag)
            {
            case HID_GLOBAL_ITEM_USAGE_PAGE:
                item.usage_page = value;
                break;

            case HID_GLOBAL_ITEM_LOGICAL_MINIMUM:
                item.logical_min = signed_value;
                break;

            case HID_GLOBAL_ITEM_LOGICAL_MAXIMUM:
                item.logical_max = signed_value;
                break;

            case HID_GLOBAL_ITEM_PHYSICAL_MINIMUM:
                item.physical_min = signed_value;
                break;

            case HID_GLOBAL_ITEM_PHYSICAL_MAXIMUM:
                item.physical_max = signed_value;
                break;

            case HID_GLOBAL_ITEM_UNIT_EXPONENT:
                item.unit_exponent = signed_value;
                break;

            case HID_GLOBAL_ITEM_UNIT:
                item.unit = value;
                break;

            case HID_GLOBAL_ITEM_REPORT_SIZE:
                item.size = value;
                break;

            case HID_GLOBAL_ITEM_REPORT_ID:
            {
                uint8_t report_id = value & 0xFF;
                hid->has_report_id = true;
                if (report_id == 0) goto free_items;
                if (report_id_index >= USB_ARRAY_SIZE(hid->report_item_list)) goto free_items;
                if (!last_report_id && has_item) goto free_items;
                hid_item_list_t *report_item_list = &hid->report_item_list[report_id_index];
                report_item_list->report_id = last_report_id;
                if (last_report_id) report_id_index++;
                memcpy(report_item_list->item_list, item_list, sizeof(item_list));
                memset(item_list, 0, sizeof(item_list));
                memset(report_bit_offset, 0, sizeof(report_bit_offset));
                last_report_id = report_id;
                break;
            }

            case HID_GLOBAL_ITEM_REPORT_COUNT:
                item.count = value;
                break;

            case HID_GLOBAL_ITEM_PUSH:
            {
                if (stack_depth >= USB_ARRAY_SIZE(stack_item)) goto free_items;
                hid_report_item_t *stack_item_ptr = &stack_item[stack_depth];
                stack_item_ptr->usage_page = item.usage_page;
                stack_item_ptr->logical_min = item.logical_min;
                stack_item_ptr->logical_max = item.logical_max;
                stack_item_ptr->physical_min = item.physical_min;
                stack_item_ptr->physical_max = item.physical_max;
                stack_item_ptr->unit_exponent = item.unit_exponent;
                stack_item_ptr->unit = item.unit;
                stack_item_ptr->size = item.size;
                stack_item_ptr->count = item.count;
                stack_depth++;
                break;
            }

            case HID_GLOBAL_ITEM_POP:
            {
                if (stack_depth == 0) goto free_items;
                stack_depth--;
                hid_report_item_t *stack_item_ptr = &stack_item[stack_depth];
                item.usage_page = stack_item_ptr->usage_page;
                item.logical_min = stack_item_ptr->logical_min;
                item.logical_max = stack_item_ptr->logical_max;
                item.physical_min = stack_item_ptr->physical_min;
                item.physical_max = stack_item_ptr->physical_max;
                item.unit_exponent = stack_item_ptr->unit_exponent;
                item.unit = stack_item_ptr->unit;
                item.size = stack_item_ptr->size;
                item.count = stack_item_ptr->count;
                break;
            }

            default:
                goto free_items;
            }
        }
        else if (item_type == HID_ITEM_TYPE_LOCAL)
        {
            switch (item_tag)
            {
            case HID_LOCAL_ITEM_USAGE:
            {
                uint16_t usage = value & 0xFFFF;
                if (item_size >= 4) local_usage_page = value >> 16;
                item.usage_min = USB_MIN(item.usage_min, usage);
                item.usage_max = USB_MAX(item.usage_max, usage);
                break;
            }

            case HID_LOCAL_ITEM_USAGE_MINIMUM:
            {
                uint16_t usage = value & 0xFFFF;
                if (item_size >= 4) local_usage_page = value >> 16;
                item.usage_min = usage;
                break;
            }

            case HID_LOCAL_ITEM_USAGE_MAXIMUM:
            {
                uint16_t usage = value & 0xFFFF;
                if (item_size >= 4) local_usage_page = value >> 16;
                item.usage_max = usage;
                break;
            }

            case HID_LOCAL_ITEM_DESIGNATOR_INDEX:
            case HID_LOCAL_ITEM_DESIGNATOR_MINIMUM:
            case HID_LOCAL_ITEM_DESIGNATOR_MAXIMUM:
            case HID_LOCAL_ITEM_STRING_INDEX:
            case HID_LOCAL_ITEM_STRING_MINIMUM:
            case HID_LOCAL_ITEM_STRING_MAXIMUM:
            case HID_LOCAL_ITEM_DELIMITER:
                break;

            default:
                goto free_items;
            }
        }
        else
        {
            goto free_items;
        }

        curr += item_size + 1;
    }

    if (!has_item) goto free_items;
    if (nesting_depth != 0) goto free_items;
    if (report_id_index >= USB_ARRAY_SIZE(hid->report_item_list)) goto free_items;
    hid_item_list_t *report_item_list = &hid->report_item_list[report_id_index++];
    report_item_list->report_id = last_report_id;
    memcpy(report_item_list->item_list, item_list, sizeof(item_list));
    printf_report_item(hid);
    return true;

free_items:
    for (size_t i = 0; i < USB_ARRAY_SIZE(item_list); i++)
    {
        hid_report_item_t *item = item_list[i];
        while (item)
        {
            hid_report_item_t *next = item->next;
            item->used = false;
            item = next;
        }
    }
    return false;
}

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length)
{
    hidh_handle_t *hid = (hidh_handle_t *)handle;
    if (!rst && hid->enum_stage != ENUM_STAGE_SET_IDLE)
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
                if (hid->hid_desc->descriptor[i].wReportLength > sizeof(public_ctrl_buf)) break;

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
        hid_drv_unbind_cb(hid->dev, hid->itf_desc->bInterfaceNumber);
        break;
    }
}

static void hid_run_task(hidh_handle_t *hid)
{
    if (hid->in_ep)
    {
        usbh_endp_read(hid->dev, hid->in_ep->bEndpointAddress, hid->read_buf, sizeof(hid->read_buf), 0);
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

void hidh_drv_register_keyboard_changed_cb(hidh_keyboard_changed_cb cb)
{
    keyboard_changed_cb = cb;
}

void hidh_drv_register_mouse_changed_cb(hidh_mouse_changed_cb cb)
{
    mouse_changed_cb = cb;
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

static void handle_free(hidh_handle_t *hid)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(hid->report_item_list); i++)
    {
        hid_item_list_t *item_list = &hid->report_item_list[i];

        for (uint8_t j = 0; j < USB_ARRAY_SIZE(item_list->item_list); j++)
        {
            if (!item_list->item_list[j]) continue;
            hid_report_item_t *item = item_list->item_list[j];
            while (item)
            {
                item->used = false;
                item = item->next;
            }
        }
    }
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
            handle_free(hid);
            memset(hid, 0, sizeof(hidh_handle_t));
        }
        hid = next;
    }
    return true;
}

static void hid_report_data_parse(hidh_handle_t *hid, const uint8_t *data, size_t len, hid_report_item_t *item_list)
{
}

static void hid_drv_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    if (!rst || !len) return;
    hidh_handle_t *hid = (hidh_handle_t *)handle;
    const uint8_t *data = (const uint8_t *)buf;

    if (hid->has_report_id)
    {
        for (size_t i = 0; i < USB_ARRAY_SIZE(hid->report_item_list); i++)
        {
            const hid_item_list_t *item_list = &hid->report_item_list[i];
            if (i && !item_list->report_id) break;
            if (item_list->report_id != data[0]) continue;
            hid_report_data_parse(hid, data, len, item_list->item_list[REPORT_TYPE_INPUT]);
            break;
        }
    }
    else
    {
        hid_report_data_parse(hid, data, len, hid->report_item_list[0].item_list[REPORT_TYPE_INPUT]);
    }
}

static void hid_drv_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
}

#endif // USB_CLASS_HIDH_DRIVER_EN
