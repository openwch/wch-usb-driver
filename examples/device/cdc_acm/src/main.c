/**
 * @file main.c
 * @author Links (lhd@wch.cn)
 * @brief Main program for CDC-ACM example
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include*/
#include <assert.h>
#include <string.h>

#include "board.h"
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

/* @global */
static usbd_handle_t *usbd_handle;
static cdcd_acm_handle_t acm_handle;
static __attribute__((aligned(4))) uint8_t transfer_buf[512];

static const void *get_string_desc_cb(uint8_t string_index, size_t *len)
{
    static uint8_t string_desc_buf[256];
    static const char *string_desc[] = {
        "wch.cn",
        "CDC-ACM Example",
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
    static uint8_t other_speed_desc[sizeof(config_desc_hs)];

    switch (desc_type)
    {
    case USB_DESC_DEVICE:
        *len = sizeof(device_desc);
        return (const void *)&device_desc;

    case USB_DESC_CONFIGURATION:
        *len = sizeof(config_desc_hs);
        return desc_info == USB_SPEED_HIGH ? (const void *)&config_desc_hs : (const void *)&config_desc_fs;

    case USB_DESC_STRING:
        return get_string_desc_cb(desc_info, len);

    case USB_DESC_DEVICE_QUALIFIER:
        *len = sizeof(qua_desc);
        return (const void *)&qua_desc;

    case USB_DESC_OTHER_SPEED_CONFIG:
        memcpy(other_speed_desc, desc_info == USB_SPEED_HIGH ? config_desc_fs : config_desc_hs, sizeof(config_desc_hs));
        other_speed_desc[1] = USB_DESC_OTHER_SPEED_CONFIG;
        *len = sizeof(other_speed_desc);
        return (const void *)other_speed_desc;
    }

    return NULL;
}

static void cdc_acm_set_line_state_cb(cdcd_acm_handle_t *cdcd_acm, uint16_t bitmap)
{
    led_write(bitmap & 0x03);
}

static void cdc_acm_read_cb(cdcd_acm_handle_t *cdcd_acm, void *buf, size_t len)
{
    /* Echo the received data back to the host */
    cdcd_acm_drv_write(&acm_handle, transfer_buf, len);
}

static void cdc_acm_write_cb(cdcd_acm_handle_t *cdcd_acm, const void *buf, size_t len)
{
    /* Start the next read operation */
    cdcd_acm_drv_read(&acm_handle, transfer_buf, sizeof(transfer_buf));
}

static void enum_completed_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    /* Initialize CDC-ACM device handle */
    memset(&acm_handle, 0, sizeof(acm_handle));
    acm_handle.usbd_handle = h;
    acm_handle.ctrl_itf_num = 0;
    acm_handle.data_itf_num = 1;
    acm_handle.read_comp_cb = cdc_acm_read_cb;
    acm_handle.write_comp_cb = cdc_acm_write_cb;
    acm_handle.set_control_line_state_cb = cdc_acm_set_line_state_cb;
    if (ctx->enum_completed.link_speed == USB_SPEED_HIGH)
    {
        acm_handle.notify_ep = (const usb_desc_endpoint_t *)&config_desc_hs[37];
        acm_handle.in_ep = (const usb_desc_endpoint_t *)&config_desc_hs[53];
        acm_handle.out_ep = (const usb_desc_endpoint_t *)&config_desc_hs[60];
    }
    else
    {
        acm_handle.notify_ep = (const usb_desc_endpoint_t *)&config_desc_fs[37];
        acm_handle.in_ep = (const usb_desc_endpoint_t *)&config_desc_fs[53];
        acm_handle.out_ep = (const usb_desc_endpoint_t *)&config_desc_fs[60];
    }
    assert(cdcd_acm_drv_open(&acm_handle));

    /* Start the first read operation */
    cdcd_acm_drv_read(&acm_handle, transfer_buf, sizeof(transfer_buf));
}

int main(void)
{
    board_init();
    led_init();

    usbd_handle = board_usb_init(USBD_INDEX, USB_MODE_DEVICE);
    assert(usbd_handle != NULL);

    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));
    assert(usbd_drv_open(usbd_handle, USB_SPEED_HIGH, false, get_stand_desc_cb));

    while (1);

    return 0;
}
