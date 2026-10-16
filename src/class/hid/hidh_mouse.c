/********************************** (C) COPYRIGHT *******************************
* File Name          : hidh_mouse.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/15
* Description        : USB HID host mouse class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "HIDH_MOUSE"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"
#include "class/hid/hidh_private.h"

#ifdef USB_CLASS_HIDH_MOUSE_EN

#ifndef USB_CLASS_HIDH_MOUSE_MAX_COUNT
#define USB_CLASS_HIDH_MOUSE_MAX_COUNT 1
#endif

/* @struct */
typedef struct mouse_handle
{
    struct mouse_handle *next;
    bool used;
    uint8_t button;
    int32_t dx;
    int32_t dy;
    int32_t wheel;
} mouse_handle_t;

/* @global */
hidh_mouse_status_t hidh_mouse_status = {0};
static hidh_mouse_changed_cb changed_cb = NULL;
static mouse_handle_t *mouse_list = NULL;
static mouse_handle_t handle_pool[USB_CLASS_HIDH_MOUSE_MAX_COUNT] = {0};

void hidh_register_mouse_changed_cb(hidh_mouse_changed_cb cb)
{
    changed_cb = cb;
}

void hidh_unregister_mouse_changed_cb(void)
{
    changed_cb = NULL;
}

void hidh_mouse_task(void)
{
    int32_t wheel = 0;
    hidh_mouse_status_t status;
    status.buttons = 0;
    status.x = hidh_mouse_status.x;
    status.y = hidh_mouse_status.y;

    mouse_handle_t *mouse = mouse_list;
    while (mouse)
    {
        status.buttons |= mouse->button;
        status.x += mouse->dx;
        status.y += mouse->dy;
        mouse->dx = 0;
        mouse->dy = 0;
        wheel += mouse->wheel;
        mouse->wheel = 0;

        mouse = mouse->next;
    }

    if (status.buttons != hidh_mouse_status.buttons || status.x != hidh_mouse_status.x ||
        status.y != hidh_mouse_status.y || wheel != 0)
    {
        hidh_mouse_change_t change;
        change.dx = status.x - hidh_mouse_status.x;
        change.dy = status.y - hidh_mouse_status.y;
        change.wheel = wheel;

        hidh_mouse_status.x = status.x;
        hidh_mouse_status.y = status.y;
        hidh_mouse_status.buttons = status.buttons;

        if (changed_cb)
        {
            changed_cb(&hidh_mouse_status, &change);
        }
    }
}

void *hidh_mouse_create(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(handle_pool); i++)
    {
        mouse_handle_t *mouse = &handle_pool[i];
        if (!mouse->used)
        {
            memset(mouse, 0, sizeof(mouse_handle_t));
            usbh_list_append((void **)&mouse_list, mouse);
            mouse->used = true;
            return mouse;
        }
    }
    return NULL;
}

void hidh_mouse_delete(void *handle)
{
    mouse_handle_t *mouse = (mouse_handle_t *)handle;
    if (mouse)
    {
        mouse->used = false;
        usbh_list_remove((void **)&mouse_list, mouse);
        memset(mouse, 0, sizeof(mouse_handle_t));
    }
}

static void usage_input_parse(mouse_handle_t *mouse, uint16_t usage_page, uint16_t usage, int32_t value)
{
    if (usage_page == HID_USAGE_PAGE_BUTTON)
    {
        if (0 < usage && usage <= 8)
        {
            mouse->button |= (value ? 1U : 0U) << (usage - 1);
        }
    }
    else if (usage_page == HID_USAGE_PAGE_DESKTOP)
    {
        switch (usage)
        {
        case HID_DESKTOP_USAGE_X:
            mouse->dx = value;
            break;
        case HID_DESKTOP_USAGE_Y:
            mouse->dy = value;
            break;
        case HID_DESKTOP_USAGE_WHEEL:
            mouse->wheel = value;
            break;
        }
    }
}

static void variable_input_parse(mouse_handle_t *mouse, const report_item_t *item, const uint8_t *data, uint32_t offset)
{
    uint16_t *usage_list = hidh_variable_usage_parse(item);
    if (!usage_list) return;

    for (uint32_t i = 0; i < item->global_info.count; i++)
    {
        bool is_signed = item->global_info.logical_min < 0;
        uint32_t curr_offset = offset + i * item->global_info.size;
        int32_t value = hidh_input_get_field(data, curr_offset, item->global_info.size, is_signed);
        uint16_t usage = usage_list[i];
        usage_input_parse(mouse, item->global_info.usage_page, usage, value);
    }
}

static void array_input_parse(mouse_handle_t *mouse, const report_item_t *item, const uint8_t *data, uint32_t offset)
{
    uint16_t usage_min = 0;
    uint16_t usage_max = 0;
    if (!hidh_array_usage_parse(item, &usage_min, &usage_max)) return;

    for (uint32_t i = 0; i < item->global_info.count; i++)
    {
        bool is_signed = item->global_info.logical_min < 0;
        uint32_t curr_offset = offset + i * item->global_info.size;
        uint16_t usage = usage_min + hidh_input_get_field(data, curr_offset, item->global_info.size, is_signed);
        usage = USB_MIN(usage, usage_max);
        if (usage)
        {
            usage_input_parse(mouse, item->global_info.usage_page, usage, 1);
        }
    }
}

void hidh_mouse_input_parse(void *handle, const report_item_t *item, const uint8_t *data, size_t len)
{
    uint32_t bit_offset = 0;
    mouse_handle_t *mouse = (mouse_handle_t *)handle;
    mouse->button = 0;

    while (item)
    {
        if ((bit_offset + item->global_info.size * item->global_info.count) >> 3 > len) break;

        if (!(item->flags & HID_ITEM_INPUT_FLAG_CONSTANT))
        {
            if (item->flags & HID_ITEM_INPUT_FLAG_VARIABLE)
            {
                variable_input_parse(mouse, item, data, bit_offset);
            }
            else
            {
                array_input_parse(mouse, item, data, bit_offset);
            }
        }

        bit_offset += item->global_info.size * item->global_info.count;
        item = item->next;
    }
}

#endif // USB_CLASS_HIDH_MOUSE_EN
