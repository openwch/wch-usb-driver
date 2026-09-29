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

#include "usb_driver.h"
#include "descriptor.h"

/* @global */
static hidd_handle_t hidd_handles[2];
static uint8_t keyboard_report_buf[8];

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

static const void *get_stand_desc_cb(uint8_t desc_type, uint8_t desc_info, size_t *len)
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
        *len = sizeof(qualifier_desc);
        return (const void *)&qualifier_desc;
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
    printf("Keyboard LED status is %02x\r\n", *(uint8_t *)buf);
}

static void enum_completed_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    hidd_handle_t *hidd = NULL;

    /* Initialize HID device handle for the keyboard interface */
    hidd = &hidd_handles[0];
    memset(hidd, 0, sizeof(hidd_handle_t));
    hidd->usbd_handle = h;
    hidd->itf_num = 0;
    hidd->in_ep = (usb_desc_endpoint_t *)&config_desc[27];
    hidd->report_buf = keyboard_report_buf;
    hidd->report_buf_size = sizeof(keyboard_report_buf);
    hidd->get_desc_cb = get_hid_desc_cb;
    hidd->set_report_comp_cb = keyboard_set_report;
    assert(hidd_drv_open(hidd));

    /* Initialize HID device handle for the mouse interface */
    hidd = &hidd_handles[1];
    memset(hidd, 0, sizeof(hidd_handle_t));
    hidd->usbd_handle = h;
    hidd->itf_num = 1;
    hidd->in_ep = (usb_desc_endpoint_t *)&config_desc[52];
    hidd->get_desc_cb = get_hid_desc_cb;
    assert(hidd_drv_open(hidd));
}

int main(void)
{
    board_init();

    usbd_handle_t *h = board_usbd_init(0);
    assert(h != NULL);

    assert(usbd_drv_open(h, USB_SPEED_FULL, false, get_stand_desc_cb));
    assert(usbd_register_event_callback(h, USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));

    while (1);

    return 0;
}
