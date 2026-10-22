/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : Main program for CDC-ACM example
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include*/
#include <assert.h>
#include <string.h>

#include "board.h"
#include "usart.h"
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

#define READ_BUF_COUNT  4
#define READ_BUF_SIZE   512

#define WRITE_BUF_COUNT 4
#define WRITE_BUF_SIZE  512

/* @struct */
typedef struct
{
    bool stop;
    uint32_t load;
    uint32_t deal;
    uint32_t count;
} xfer_manage_t;

/* @global */
static volatile bool enum_completed;
static usbd_handle_t *usbd_handle;
static cdcd_acm_handle_t acm_handle;
static xfer_manage_t read_xfer_manage;
static xfer_manage_t write_xfer_manage;
static uint32_t read_length[READ_BUF_COUNT];
static uint32_t write_length[WRITE_BUF_COUNT];
static __attribute__((aligned(4))) uint8_t read_bufs[READ_BUF_COUNT][READ_BUF_SIZE];
static __attribute__((aligned(4))) uint8_t write_bufs[WRITE_BUF_COUNT][WRITE_BUF_SIZE];

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

static const void *get_stand_desc_cb(usbd_handle_t *h, uint8_t type, uint8_t index, usb_speed_t speed, size_t *len)
{
    static uint8_t other_speed_desc[sizeof(config_desc_hs)];

    switch (type)
    {
    case USB_DESC_DEVICE:
        *len = sizeof(device_desc);
        return (const void *)&device_desc;

    case USB_DESC_CONFIGURATION:
        *len = sizeof(config_desc_hs);
        return speed == USB_SPEED_HIGH ? (const void *)&config_desc_hs : (const void *)&config_desc_fs;

    case USB_DESC_STRING:
        return get_string_desc_cb(index, len);

    case USB_DESC_DEVICE_QUALIFIER:
        *len = sizeof(qua_desc);
        return (const void *)&qua_desc;

    case USB_DESC_OTHER_SPEED_CONFIG:
        memcpy(other_speed_desc, speed == USB_SPEED_HIGH ? config_desc_fs : config_desc_hs, sizeof(config_desc_hs));
        other_speed_desc[1] = USB_DESC_OTHER_SPEED_CONFIG;
        *len = sizeof(other_speed_desc);
        return (const void *)other_speed_desc;
    }

    return NULL;
}

static bool cdc_acm_set_line_coding_cb(cdcd_acm_handle_t *cdcd_acm, cdc_line_coding_t *line_coding)
{
    usart_stop_bits_t stop_bits;
    usart_parity_t parity;

    switch (line_coding->stop_bits)
    {
    case 0:
        stop_bits = USART_STOP_BITS_1;
        break;

    case 1:
        stop_bits = USART_STOP_BITS_1_5;
        break;

    case 2:
        stop_bits = USART_STOP_BITS_2;
        break;

    default:
        return false;
    }

    switch (line_coding->parity)
    {
    case 0:
        parity = USART_PARITY_NONE;
        break;

    case 1:
        parity = USART_PARITY_ODD;
        break;

    case 2:
        parity = USART_PARITY_EVEN;
        break;

    default:
        return false;
    }

    return usart_config(line_coding->bit_rate, stop_bits, parity);
}

static void cdc_acm_set_line_state_cb(cdcd_acm_handle_t *cdcd_acm, uint16_t bitmap)
{
    led_write(bitmap & 0x03);
}

static void cdc_acm_read_cb(cdcd_acm_handle_t *cdcd_acm, void *buf, size_t len)
{
    /* Store the length of the received data */
    read_length[read_xfer_manage.load] = len;
    read_xfer_manage.load = (read_xfer_manage.load + 1) % READ_BUF_COUNT;
    read_xfer_manage.count++;

    /* Start the next read operation if there is space in the buffer */
    if (read_xfer_manage.count + 1 < READ_BUF_COUNT)
    {
        cdcd_acm_drv_read(&acm_handle, read_bufs[read_xfer_manage.load], READ_BUF_SIZE);
    }
    else
    {
        read_xfer_manage.stop = true;
    }
}

static void reset_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    enum_completed = false;
    memset(&read_xfer_manage, 0, sizeof(read_xfer_manage));
    memset(&write_xfer_manage, 0, sizeof(write_xfer_manage));
}

static void enum_completed_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    enum_completed = true;
    memset(&read_xfer_manage, 0, sizeof(read_xfer_manage));
    memset(&write_xfer_manage, 0, sizeof(write_xfer_manage));

    /* Initialize CDC-ACM device handle */
    memset(&acm_handle, 0, sizeof(acm_handle));
    acm_handle.usbd_handle = h;
    acm_handle.ctrl_itf_num = 0;
    acm_handle.data_itf_num = 1;
    acm_handle.read_comp_cb = cdc_acm_read_cb;
    acm_handle.set_line_coding_cb = cdc_acm_set_line_coding_cb;
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
    cdcd_acm_drv_read(&acm_handle, read_bufs[read_xfer_manage.load], READ_BUF_SIZE);
}

static void cdc_acm_application_handle(void)
{
    /* Disable interrupts to safely access shared resources */
    board_disable_irq();

    /* Handle data read from USB and write it to USART */
    if (read_xfer_manage.count && usart_write(read_bufs[read_xfer_manage.deal], read_length[read_xfer_manage.deal]))
    {
        read_xfer_manage.deal = (read_xfer_manage.deal + 1) % READ_BUF_COUNT;
        read_xfer_manage.count--;
    }

    /* Start the next read operation if the previous one has stopped and there is space in the buffer */
    if (read_xfer_manage.stop && read_xfer_manage.count + 1 < READ_BUF_COUNT)
    {
        if (cdcd_acm_drv_read(&acm_handle, read_bufs[read_xfer_manage.load], READ_BUF_SIZE))
        {
            read_xfer_manage.stop = false;
        }
    }

    /* Re-enable interrupts after accessing shared resources */
    board_enable_irq();

    /* Handle data read from USART and prepare it for writing to USB */
    if (write_xfer_manage.count + 1 < WRITE_BUF_COUNT)
    {
        size_t length = usart_read(write_bufs[write_xfer_manage.load], WRITE_BUF_SIZE);
        if (length)
        {
            write_length[write_xfer_manage.load] = length;
            write_xfer_manage.load = (write_xfer_manage.load + 1) % WRITE_BUF_COUNT;
            write_xfer_manage.count++;
        }
    }

    if (write_xfer_manage.count &&
        cdcd_acm_drv_write(&acm_handle, write_bufs[write_xfer_manage.deal], write_length[write_xfer_manage.deal]))
    {
        write_xfer_manage.deal = (write_xfer_manage.deal + 1) % WRITE_BUF_COUNT;
        write_xfer_manage.count--;
    }
}

int main(void)
{
    board_init();
    usart_init();
    led_init();

    usbd_handle = board_usb_init(USBD_INDEX, USB_MODE_DEVICE);
    assert(usbd_handle != NULL);

    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_RESET, reset_event_cb));
    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));
    assert(usbd_drv_open(usbd_handle, USB_SPEED_HIGH, false, get_stand_desc_cb));

    while (1)
    {
        if (enum_completed)
        {
            cdc_acm_application_handle();
        }
    }

    return 0;
}
