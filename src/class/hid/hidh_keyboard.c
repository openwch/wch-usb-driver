/********************************** (C) COPYRIGHT *******************************
* File Name          : hidh_keyboard.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
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

#define LED_NUM_LOCK_BIT_MASK    (1U << 0)
#define LED_CAPS_LOCK_BIT_MASK   (1U << 1)
#define LED_SCROLL_LOCK_BIT_MASK (1U << 2)

/* @struct */
typedef struct kb_handle
{
    struct kb_handle *next;
    bool used;
    hidh_handle_t *hid;
    hidh_instance_t instance;

    uint8_t led_status;
    uint32_t button_status_bitmap[256 / 32];
} kb_handle_t;

/* @global */
hidh_keyboard_t hidh_keyboard = {0};
static kb_handle_t *kb_list = NULL;
static kb_handle_t handle_pool[USB_CLASS_HIDH_KEYBOARD_MAX_COUNT] = {0};
static hidh_keyboard_changed_cb changed_cb = NULL;

void hidh_register_keyboard_changed_cb(hidh_keyboard_changed_cb cb)
{
    changed_cb = cb;
}

void hidh_unregister_keyboard_changed_cb(void)
{
    changed_cb = NULL;
}

bool hidh_keyboard_get_key_status(uint8_t key_code)
{
    uint8_t index = key_code >> 5;
    uint8_t bit = key_code & 0x1F;
    if (index >= USB_ARRAY_SIZE(hidh_keyboard.button_status_bitmap)) return false;
    return hidh_keyboard.button_status_bitmap[index] & (1U << bit);
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
            if (hidh_set_report(kb->hid, &kb->instance))
            {
                kb->led_status = hidh_keyboard.led_status;
            }
        }
        kb = kb->next;
    }

    if (memcmp(&hidh_keyboard.button_status_bitmap, &keyboard.button_status_bitmap,
               sizeof(hidh_keyboard.button_status_bitmap)) != 0)
    {
        uint8_t last_lock_status = 0;
        if (hidh_keyboard_get_key_status(HID_KEY_CODE_NUM_LOCK)) last_lock_status |= LED_NUM_LOCK_BIT_MASK;
        if (hidh_keyboard_get_key_status(HID_KEY_CODE_CAPS_LOCK)) last_lock_status |= LED_CAPS_LOCK_BIT_MASK;
        if (hidh_keyboard_get_key_status(HID_KEY_CODE_SCROLL_LOCK)) last_lock_status |= LED_SCROLL_LOCK_BIT_MASK;

        memcpy(&hidh_keyboard.button_status_bitmap, &keyboard.button_status_bitmap,
               sizeof(hidh_keyboard.button_status_bitmap));

        uint8_t new_lock_status = 0;
        if (hidh_keyboard_get_key_status(HID_KEY_CODE_NUM_LOCK)) new_lock_status |= LED_NUM_LOCK_BIT_MASK;
        if (hidh_keyboard_get_key_status(HID_KEY_CODE_CAPS_LOCK)) new_lock_status |= LED_CAPS_LOCK_BIT_MASK;
        if (hidh_keyboard_get_key_status(HID_KEY_CODE_SCROLL_LOCK)) new_lock_status |= LED_SCROLL_LOCK_BIT_MASK;

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

static void keyboard_delete_cb(hidh_instance_t *instance)
{
    kb_handle_t *kb = (kb_handle_t *)instance->handle;
    usbh_list_remove((void **)&kb_list, kb);
    memset(kb, 0, sizeof(kb_handle_t));
}

static void keyboard_start_parse_cb(hidh_instance_t *instance)
{
    kb_handle_t *kb = (kb_handle_t *)instance->handle;
    memset(&kb->button_status_bitmap, 0, sizeof(kb->button_status_bitmap));
}

static void keyboard_get_report_cb(hidh_instance_t *instance, uint16_t page, uint16_t usage, int32_t value)
{
    kb_handle_t *kb = (kb_handle_t *)instance->handle;
    if (page == HID_USAGE_PAGE_KEYBOARD)
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

static uint32_t keyboard_set_report_cb(hidh_instance_t *instance, uint16_t page, uint16_t usage)
{
    if (page == HID_USAGE_PAGE_LED)
    {
        switch (usage)
        {
        case HID_LED_NUM_LOCK:
            return hidh_keyboard.led_status & LED_NUM_LOCK_BIT_MASK ? 1 : 0;

        case HID_LED_CAPS_LOCK:
            return hidh_keyboard.led_status & LED_CAPS_LOCK_BIT_MASK ? 1 : 0;

        case HID_LED_SCROLL_LOCK:
            return hidh_keyboard.led_status & LED_SCROLL_LOCK_BIT_MASK ? 1 : 0;
        }
    }
    return 0;
}

hidh_instance_t *hidh_keyboard_create_instance(hidh_handle_t *hid)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(handle_pool); i++)
    {
        kb_handle_t *kb = &handle_pool[i];
        if (!kb->used)
        {
            memset(kb, 0, sizeof(kb_handle_t));
            usbh_list_append((void **)&kb_list, kb);
            kb->used = true;
            kb->hid = hid;
            kb->instance.handle = kb;
            kb->instance.delete_cb = keyboard_delete_cb;
            kb->instance.start_parse_cb = keyboard_start_parse_cb;
            kb->instance.get_report_cb = keyboard_get_report_cb;
            kb->instance.set_report_cb = keyboard_set_report_cb;
            return &kb->instance;
        }
    }
    return NULL;
}

#endif // USB_CLASS_HIDH_KEYBOARD_EN
