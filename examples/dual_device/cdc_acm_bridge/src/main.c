/**
 * @file main.c
 * @author Links (lhd@wch.cn)
 * @brief Main program for CDC-ACM Bridge example
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
#ifndef USBD0_INDEX
#define USBD0_INDEX 0
#endif

#ifndef USBD1_INDEX
#define USBD1_INDEX 1
#endif

#if USBD0_INDEX == USBD1_INDEX
#error "Not supported: USBD0_INDEX and USBD1_INDEX cannot be the same"
#endif

#if USBD0_INDEX >= USB_COUNT
#error "Not supported: USBD0_INDEX exceeds USB_COUNT"
#endif

#if USBD1_INDEX >= USB_COUNT
#error "Not supported: USBD1_INDEX exceeds USB_COUNT"
#endif

/* @global */
static volatile bool enum_completed[2];
static uint8_t led_state;
static usbd_handle_t *usbd_handles[2];
static cdcd_acm_handle_t acm_handles[2];
static __attribute__((aligned(4))) uint8_t transfer_buf[2][512];

static const void *get_string_desc_cb(uint8_t string_index, size_t *len)
{
    static uint8_t string_desc_buf[256];
    static const char *string_desc[] = {
        "wch.cn",
        "CDC-ACM Bridge Example",
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
    static uint8_t other_speed_desc_buf[2][sizeof(config_desc_hs)];
    uint8_t *other_speed_desc_ptr;

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
        other_speed_desc_ptr = h == usbd_handles[0] ? other_speed_desc_buf[0] : other_speed_desc_buf[1];
        memcpy(other_speed_desc_ptr, desc_info == USB_SPEED_HIGH ? config_desc_fs : config_desc_hs,
               sizeof(config_desc_hs));
        other_speed_desc_ptr[1] = USB_DESC_OTHER_SPEED_CONFIG;
        *len = sizeof(other_speed_desc_buf[0]);
        return (const void *)other_speed_desc_ptr;
    }

    return NULL;
}

static void cdc_acm_read_cb(cdcd_acm_handle_t *cdcd_acm, void *buf, size_t len)
{
    /* Forward the received data to the another CDC-ACM device */
    if (cdcd_acm == &acm_handles[0])
    {
        if (enum_completed[1])
        {
            led_state |= LED_0;
            led_write(led_state);
            cdcd_acm_drv_write(&acm_handles[1], buf, len);
        }
        else
        {
            cdcd_acm_drv_read(&acm_handles[0], transfer_buf[0], sizeof(transfer_buf[0]));
        }
    }
    else
    {
        if (enum_completed[0])
        {
            led_state |= LED_1;
            led_write(led_state);
            cdcd_acm_drv_write(&acm_handles[0], buf, len);
        }
        else
        {
            cdcd_acm_drv_read(&acm_handles[1], transfer_buf[1], sizeof(transfer_buf[1]));
        }
    }
}

static void cdc_acm_write_cb(cdcd_acm_handle_t *cdcd_acm, const void *buf, size_t len)
{
    /* Start the next read operation */
    if (cdcd_acm == &acm_handles[0])
    {
        if (enum_completed[1])
        {
            led_state &= ~LED_1;
            led_write(led_state);
            cdcd_acm_drv_read(&acm_handles[1], transfer_buf[1], sizeof(transfer_buf[1]));
        }
        else
        {
            cdcd_acm_drv_read(&acm_handles[0], transfer_buf[0], sizeof(transfer_buf[0]));
        }
    }
    else
    {
        if (enum_completed[0])
        {
            led_state &= ~LED_0;
            led_write(led_state);
            cdcd_acm_drv_read(&acm_handles[0], transfer_buf[0], sizeof(transfer_buf[0]));
        }
        else
        {
            cdcd_acm_drv_read(&acm_handles[1], transfer_buf[1], sizeof(transfer_buf[1]));
        }
    }
}

static void reset_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    enum_completed[h == usbd_handles[0] ? 0 : 1] = false;
}

static void enum_completed_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    cdcd_acm_handle_t *cdcd_acm;
    uint8_t *buf_ptr;

    if (h == usbd_handles[0])
    {
        enum_completed[0] = true;
        cdcd_acm = &acm_handles[0];
        buf_ptr = transfer_buf[0];
    }
    else
    {
        enum_completed[1] = true;
        cdcd_acm = &acm_handles[1];
        buf_ptr = transfer_buf[1];
    }

    /* Turn off the LED to indicate enumeration completion */
    led_state = 0;

    /* Initialize CDC-ACM device handle */
    memset(cdcd_acm, 0, sizeof(cdcd_acm_handle_t));
    cdcd_acm->usbd_handle = h;
    cdcd_acm->ctrl_itf_num = 0;
    cdcd_acm->data_itf_num = 1;
    cdcd_acm->read_comp_cb = cdc_acm_read_cb;
    cdcd_acm->write_comp_cb = cdc_acm_write_cb;
    if (ctx->enum_completed.link_speed == USB_SPEED_HIGH)
    {
        cdcd_acm->notify_ep = (const usb_desc_endpoint_t *)&config_desc_hs[37];
        cdcd_acm->in_ep = (const usb_desc_endpoint_t *)&config_desc_hs[53];
        cdcd_acm->out_ep = (const usb_desc_endpoint_t *)&config_desc_hs[60];
    }
    else
    {
        cdcd_acm->notify_ep = (const usb_desc_endpoint_t *)&config_desc_fs[37];
        cdcd_acm->in_ep = (const usb_desc_endpoint_t *)&config_desc_fs[53];
        cdcd_acm->out_ep = (const usb_desc_endpoint_t *)&config_desc_fs[60];
    }
    assert(cdcd_acm_drv_open(cdcd_acm));

    /* Start the first read operation */
    cdcd_acm_drv_read(cdcd_acm, buf_ptr, sizeof(transfer_buf[0]));
}

int main(void)
{
    board_init();
    led_init();

    usbd_handles[0] = board_usb_init(USBD0_INDEX, USB_MODE_DEVICE);
    usbd_handles[1] = board_usb_init(USBD1_INDEX, USB_MODE_DEVICE);
    assert(usbd_handles[0] != NULL && usbd_handles[1] != NULL);

    enum_completed[0] = false;
    enum_completed[1] = false;
    assert(usbd_register_event_callback(usbd_handles[0], USBD_EVENT_RESET, reset_event_cb));
    assert(usbd_register_event_callback(usbd_handles[1], USBD_EVENT_RESET, reset_event_cb));
    assert(usbd_register_event_callback(usbd_handles[0], USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));
    assert(usbd_register_event_callback(usbd_handles[1], USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));
    assert(usbd_drv_open(usbd_handles[0], USB_SPEED_HIGH, false, get_stand_desc_cb));
    assert(usbd_drv_open(usbd_handles[1], USB_SPEED_HIGH, false, get_stand_desc_cb));

    while (1);

    return 0;
}
