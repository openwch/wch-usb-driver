/********************************** (C) COPYRIGHT *******************************
* File Name          : hidh_keyboard.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/15
* Description        : USB HID host keyboard class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "HIDH_KEYBOARD"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"
#include "class/hid/hidh_private.h"

#ifdef USB_CLASS_HIDH_KEYBOARD_EN

#ifndef USB_CLASS_HIDH_KEYBOARD_MAX_COUNT
#define USB_CLASS_HIDH_KEYBOARD_MAX_COUNT 1
#endif

/* @struct */
typedef struct kb_handle
{
    struct kb_handle *next;
    bool used;
    hidh_handle_t *parent;

    uint8_t led_status;
    uint32_t button_status_bitmap[256 / 32];
} kb_handle_t;

/* @global */
hidh_keyboard_t hidh_keyboard = {0};
static hidh_keyboard_changed_cb changed_cb = NULL;
static kb_handle_t *kb_list = NULL;
static kb_handle_t handle_pool[USB_CLASS_HIDH_KEYBOARD_MAX_COUNT] = {0};

void hidh_register_keyboard_changed_cb(hidh_keyboard_changed_cb cb)
{
    changed_cb = cb;
}

void hidh_unregister_keyboard_changed_cb(void)
{
    changed_cb = NULL;
}

bool hidh_keyboard_get_status(uint8_t key_code)
{
    uint8_t index = key_code >> 5;
    uint8_t bit = key_code & 0x1F;
    if (index >= USB_ARRAY_SIZE(hidh_keyboard.button_status_bitmap)) return false;
    return hidh_keyboard.button_status_bitmap[index] & (1U << bit);
}

static void keyboard_set_led_status(kb_handle_t *kb)
{
}

void hidh_keyboard_task(void)
{
    hidh_keyboard_t keyboard;
    memset(&keyboard.button_status_bitmap, 0, sizeof(keyboard.button_status_bitmap));

    kb_handle_t *kb = kb_list;
    while (kb)
    {
        for (size_t i = 0; i < USB_ARRAY_SIZE(kb->button_status_bitmap); i++)
        {
            keyboard.button_status_bitmap[i] |= kb->button_status_bitmap[i];
        }

        if (kb->led_status != hidh_keyboard.led_status)
        {
            kb->led_status = hidh_keyboard.led_status;
            keyboard_set_led_status(kb);
        }
        kb = kb->next;
    }

    if (memcmp(&hidh_keyboard.button_status_bitmap, &keyboard.button_status_bitmap,
               sizeof(hidh_keyboard.button_status_bitmap)) != 0)
    {
        uint8_t last_lock_status = 0;
        if (hidh_keyboard_get_status(HID_KEY_CODE_NUM_LOCK)) last_lock_status |= 1U << 0;
        if (hidh_keyboard_get_status(HID_KEY_CODE_CAPS_LOCK)) last_lock_status |= 1U << 1;
        if (hidh_keyboard_get_status(HID_KEY_CODE_SCROLL_LOCK)) last_lock_status |= 1U << 2;

        memcpy(&hidh_keyboard.button_status_bitmap, &keyboard.button_status_bitmap,
               sizeof(hidh_keyboard.button_status_bitmap));

        uint8_t new_lock_status = 0;
        if (hidh_keyboard_get_status(HID_KEY_CODE_NUM_LOCK)) new_lock_status |= 1U << 0;
        if (hidh_keyboard_get_status(HID_KEY_CODE_CAPS_LOCK)) new_lock_status |= 1U << 1;
        if (hidh_keyboard_get_status(HID_KEY_CODE_SCROLL_LOCK)) new_lock_status |= 1U << 2;

        uint8_t led_change = (last_lock_status ^ new_lock_status) & new_lock_status;
        if (led_change)
        {
            hidh_keyboard.led_status ^= led_change;
        }

        if (changed_cb)
        {
            changed_cb(&hidh_keyboard);
        }
    }
}

void *hidh_keyboard_create(hidh_handle_t *parent)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(handle_pool); i++)
    {
        kb_handle_t *kb = &handle_pool[i];
        if (!kb->used)
        {
            memset(kb, 0, sizeof(kb_handle_t));
            usbh_list_append((void **)&kb_list, kb);
            kb->used = true;
            kb->parent = parent;
            return kb;
        }
    }
    return NULL;
}

void hidh_keyboard_delete(void *handle)
{
    kb_handle_t *kb = (kb_handle_t *)handle;
    if (kb)
    {
        kb->used = false;
        usbh_list_remove((void **)&kb_list, kb);
        memset(kb, 0, sizeof(kb_handle_t));
    }
}

void keyboard_input_start_parse(void *handle)
{
    kb_handle_t *kb = (kb_handle_t *)handle;
    memset(&kb->button_status_bitmap, 0, sizeof(kb->button_status_bitmap));
}

void keyboard_input_usage_parse(void *handle, uint16_t usage_page, uint16_t usage, int32_t value)
{
    kb_handle_t *kb = (kb_handle_t *)handle;
    if (usage_page == HID_USAGE_PAGE_KEYBOARD)
    {
        if (usage)
        {
            uint8_t index = usage >> 5;
            uint8_t bit = usage & 0x1F;
            if (index >= USB_ARRAY_SIZE(kb->button_status_bitmap)) return;
            kb->button_status_bitmap[index] |= (value ? 1U : 0U) << bit;
        }
    }
}

#endif // USB_CLASS_HIDH_KEYBOARD_EN
