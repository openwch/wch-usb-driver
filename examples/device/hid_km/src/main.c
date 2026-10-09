/**
 * @file main.c
 * @author Links (lhd@wch.cn)
 * @brief Main program for HID keyboard and mouse example
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include*/
#include <assert.h>
#include <string.h>

#include "board.h"
#include "button.h"
#include "led.h"

#include "usb_driver.h"
#include "descriptor.h"

/* @define */
#ifndef USBD_INDEX
#define USBD_INDEX 0
#endif

#if USBD_INDEX >= USB_COUNT
#error "Not supported: USBD_INDEX exceeds USB_COUNT"
#endif

/* @struct */
typedef struct
{
    uint8_t modifier;
    uint8_t reserved;
    uint8_t key_code[6];
} hid_kb_report_t;

typedef struct
{
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} hid_mouse_report_t;

/* @global */
static volatile bool enum_completed;
static volatile bool is_suspended;
static usbd_handle_t *usbd_handle;
static hidd_handle_t kb_handle;
static hidd_handle_t mouse_handle;
static hid_kb_report_t hid_kb_report;
static hid_mouse_report_t hid_mouse_report;
static __attribute__((aligned(4))) uint8_t hid_kb_led_status;

static const void *get_string_desc_cb(uint8_t string_index, size_t *len)
{
    static uint8_t string_desc_buf[256];
    static const char *string_desc[] = {
        "wch.cn",
        "HID Keyboard and Mouse Example",
        "0123456789",
    };

    if (string_index == 0)
    {
        static const uint8_t lang_id_desc[] = {0x04, 0x03, 0x09, 0x04};
        *len = sizeof(lang_id_desc);
        return lang_id_desc;
    }
    else if (string_index <= USB_ARRAY_SIZE(string_desc))
    {
        const char *str = string_desc[string_index - 1];
        *len = strlen(str) * 2 + 2;
        string_desc_buf[0] = *len;
        string_desc_buf[1] = USB_DESC_STRING;
        for (size_t i = 0; i < strlen(str); i++)
        {
            string_desc_buf[2 + i * 2] = str[i];
            string_desc_buf[3 + i * 2] = 0;
        }
        return (const void *)string_desc_buf;
    }

    return NULL;
}

static const void *get_stand_desc_cb(usbd_handle_t *h, uint8_t desc_type, uint8_t desc_info, size_t *len)
{
    switch (desc_type)
    {
    case USB_DESC_DEVICE:
        *len = sizeof(device_desc);
        return (const void *)&device_desc;

    case USB_DESC_CONFIGURATION:
        *len = sizeof(config_desc);
        return (const void *)&config_desc;

    case USB_DESC_STRING:
        return get_string_desc_cb(desc_info, len);

    case USB_DESC_DEVICE_QUALIFIER:
        *len = sizeof(qua_desc);
        return (const void *)&qua_desc;
    }

    return NULL;
}

static bool get_hid_desc_cb(hidd_handle_t *hidd, uint8_t desc_type, uint8_t desc_index, void **desc, size_t *len)
{
    if (hidd->itf_num == 0)
    {
        switch (desc_type)
        {
        case HID_DESC_HID:
            *desc = (void *)&config_desc[18];
            *len = config_desc[18];
            return true;

        case HID_DESC_REPORT:
            *desc = (void *)keyboard_report_desc;
            *len = sizeof(keyboard_report_desc);
            return true;
        }
    }
    else if (hidd->itf_num == 1)
    {
        switch (desc_type)
        {
        case HID_DESC_HID:
            *desc = (void *)&config_desc[43];
            *len = config_desc[43];
            return true;

        case HID_DESC_REPORT:
            *desc = (void *)mouse_report_desc;
            *len = sizeof(mouse_report_desc);
            return true;
        }
    }
    return false;
}

static void keyboard_set_report(hidd_handle_t *hidd, uint8_t type, uint8_t id, void *buf, size_t len)
{
    uint8_t led_status = *(uint8_t *)buf & 0x07;
    led_write(led_status);
    printf("Keyboard LED status is %02x\r\n", led_status);
}

static void reset_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    enum_completed = false;
    is_suspended = false;
    memset(&kb_handle, 0, sizeof(hidd_handle_t));
    memset(&mouse_handle, 0, sizeof(hidd_handle_t));
    memset(&hid_kb_report, 0, sizeof(hid_kb_report));
    memset(&hid_mouse_report, 0, sizeof(hid_mouse_report));
    memset(&hid_kb_led_status, 0, sizeof(hid_kb_led_status));
}

static void suspend_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    is_suspended = true;
}

static void enum_completed_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    enum_completed = true;

    /* Initialize HID device handle for the keyboard interface */
    memset(&kb_handle, 0, sizeof(hidd_handle_t));
    kb_handle.usbd_handle = h;
    kb_handle.itf_num = 0;
    kb_handle.in_ep = (const usb_desc_endpoint_t *)&config_desc[27];
    kb_handle.report_buf = &hid_kb_led_status;
    kb_handle.report_buf_size = sizeof(hid_kb_led_status);
    kb_handle.get_desc_cb = get_hid_desc_cb;
    kb_handle.set_report_comp_cb = keyboard_set_report;
    assert(hidd_drv_open(&kb_handle));

    /* Initialize HID device handle for the mouse interface */
    memset(&mouse_handle, 0, sizeof(hidd_handle_t));
    mouse_handle.usbd_handle = h;
    mouse_handle.itf_num = 1;
    mouse_handle.in_ep = (const usb_desc_endpoint_t *)&config_desc[52];
    mouse_handle.get_desc_cb = get_hid_desc_cb;
    assert(hidd_drv_open(&mouse_handle));
}

static void hid_kb_button_handle(hid_kb_report_t *report, uint8_t key_code, bool pressed)
{
    /* Modifier keys */
    if (key_code >= HID_KEY_CODE_CONTROL_LEFT && key_code <= HID_KEY_CODE_GUI_RIGHT)
    {
        uint8_t bit_mask = 1 << (key_code & 0x07);
        report->modifier = pressed ? (report->modifier | bit_mask) : (report->modifier & ~bit_mask);
    }
    /* Normal keys press */
    else if (pressed)
    {
        for (size_t i = 0; i < sizeof(report->key_code); i++)
        {
            if (report->key_code[i] == key_code)
            {
                break;
            }

            if (report->key_code[i] == HID_KEY_CODE_NONE)
            {
                report->key_code[i] = key_code;
                break;
            }
        }
    }
    /* Normal keys leave */
    else
    {
        for (size_t i = 0; i < sizeof(report->key_code); i++)
        {
            if (report->key_code[i] == key_code)
            {
                report->key_code[i] = HID_KEY_CODE_NONE;
                memmove(&report->key_code[i], &report->key_code[i + 1], sizeof(report->key_code) - i - 1);
                report->key_code[sizeof(report->key_code) - 1] = HID_KEY_CODE_NONE;
                break;
            }
        }
    }
}

static void hid_application_handle(void)
{
    static const uint8_t keycode_map[4] = {
        HID_KEY_CODE_A,
        HID_KEY_CODE_B,
        HID_KEY_CODE_CONTROL_LEFT,
        HID_KEY_CODE_SHIFT_LEFT,
    };

    uint8_t button_status = button_read();

    /* HID keyboard handling */
    hid_kb_report_t temp_kb_report;
    memcpy(&temp_kb_report, &hid_kb_report, sizeof(hid_kb_report_t));

    for (size_t i = 0; i < USB_ARRAY_SIZE(keycode_map); i++)
    {
        hid_kb_button_handle(&temp_kb_report, keycode_map[i], (button_status & (1 << i)) ? true : false);
    }

    if (memcmp(&hid_kb_report, &temp_kb_report, sizeof(hid_kb_report_t)) != 0)
    {
        /* Resume the USB device if it was suspended */
        if (is_suspended)
        {
            usbd_drv_resume(kb_handle.usbd_handle);
            is_suspended = false;
        }

        memcpy(&hid_kb_report, &temp_kb_report, sizeof(hid_kb_report_t));
        hidd_drv_write(&kb_handle, &hid_kb_report, sizeof(hid_kb_report_t));
    }

    /* HID mouse handling */
    int8_t dx = 0;
    int8_t dy = 0;

    if (button_status & BUTTON_4)
    {
        dx += 1;
    }

    if (button_status & BUTTON_5)
    {
        dx -= 1;
    }

    if (button_status & BUTTON_6)
    {
        dy += 1;
    }

    if (button_status & BUTTON_7)
    {
        dy -= 1;
    }

    if (dx || dy)
    {
        /* Resume the USB device if it was suspended */
        if (is_suspended)
        {
            usbd_drv_resume(mouse_handle.usbd_handle);
            is_suspended = false;
        }

        hid_mouse_report.x = dx;
        hid_mouse_report.y = dy;
        hidd_drv_write(&mouse_handle, &hid_mouse_report, sizeof(hid_mouse_report_t));
    }
}

int main(void)
{
    board_init();
    button_init();
    led_init();

    usbd_handle = board_usb_init(USBD_INDEX, USB_MODE_DEVICE);
    assert(usbd_handle != NULL);

    enum_completed = false;
    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_RESET, reset_event_cb));
    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_SUSPEND, suspend_event_cb));
    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));
    assert(usbd_drv_open(usbd_handle, USB_SPEED_FULL, false, get_stand_desc_cb));

    while (1)
    {
        if (enum_completed)
        {
            hid_application_handle();
        }
    }

    return 0;
}
