/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : Main program for UAC Headset example
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include*/
#include <assert.h>
#include <string.h>

#include "board.h"
#include "adc.h"
#include "dac.h"

#include "usb_driver.h"
#include "descriptor.h"

/* @define */
#ifndef USBD_INDEX
#define USBD_INDEX 0
#endif

#if USBD_INDEX >= USB_COUNT
#error "Not supported: USBD_INDEX exceeds USB_COUNT"
#endif

#define STREAM_INPUT_ITF_NUM  1
#define STREAM_OUTPUT_ITF_NUM 2
#define STREAM_INPUT_ENDP     ((const uac_desc_audio_endpoint_t *)&config_desc[106])
#define STREAM_OUTPUT_ENDP    ((const uac_desc_audio_endpoint_t *)&config_desc[158])

/* @global */
static usbd_handle_t *usbd_handle;
static uacd_handle_t uac_handle;
static uint8_t stream_input_alt_setting;
static uint8_t stream_output_alt_setting;
static __attribute__((aligned(4))) uint8_t read_buf[200];
static __attribute__((aligned(4))) uint8_t write_buf[100];

static const void *get_string_desc_cb(uint8_t index, size_t *len)
{
    static __attribute__((aligned(4))) uint8_t string_desc_buf[128];

    uint8_t char_len;
    uint16_t *char_ptr = (uint16_t *)&string_desc_buf[2];

    if (index == 0)
    {
        char_len = 1;
        char_ptr[0] = 0x0409;
    }
    else if (index == 3)
    {
        static const char ascii_char[] = "0123456789ABCDEF";
        const uint8_t *mac_addr = board_get_mac_address();
        char_len = 12;
        for (uint8_t i = 0; i < 6; i++)
        {
            char_ptr[i * 2 + 0] = ascii_char[(mac_addr[i] >> 4) & 0x0F];
            char_ptr[i * 2 + 1] = ascii_char[(mac_addr[i] >> 0) & 0x0F];
        }
    }
    else
    {
        static const char *string_desc[] = {
            "wch.cn",
            "UAC Headset Example",
        };

        uint8_t string_index = index - 1;

        if (string_index < USB_ARRAY_SIZE(string_desc))
        {
            char_len = strlen(string_desc[string_index]);
            for (uint8_t i = 0; i < char_len; i++)
            {
                char_ptr[i] = string_desc[string_index][i];
            }
        }
        else
        {
            *len = 0;
            return NULL;
        }
    }

    uint8_t desc_len = char_len * 2 + 2;
    *len = desc_len;
    string_desc_buf[0] = desc_len;
    string_desc_buf[1] = USB_DESC_STRING;
    return string_desc_buf;
}

static const void *get_stand_desc_cb(usbd_handle_t *h, uint8_t type, uint8_t index, usb_speed_t speed, size_t *len)
{
    switch (type)
    {
    case USB_DESC_DEVICE:
        *len = sizeof(device_desc);
        return (const void *)&device_desc;

    case USB_DESC_CONFIGURATION:
        *len = sizeof(config_desc);
        return (const void *)&config_desc;

    case USB_DESC_STRING:
        return get_string_desc_cb(index, len);

    case USB_DESC_DEVICE_QUALIFIER:
        *len = sizeof(qua_desc);
        return (const void *)&qua_desc;
    }

    return NULL;
}

static void uac_set_itf_cb(uacd_handle_t *uacd, uint8_t itf_num, uint8_t alt_setting)
{
    switch (itf_num)
    {
    case STREAM_INPUT_ITF_NUM:
        stream_input_alt_setting = alt_setting;
        if (alt_setting == 0)
        {
            /* Close the input stream endpoint for audio capture */
            uacd_drv_close_stream_endpoint(uacd, STREAM_INPUT_ENDP);
        }
        else
        {
            /* Open the input stream endpoint for audio capture */
            uacd_drv_open_stream_endpoint(uacd, STREAM_INPUT_ENDP);
            uacd_drv_read(uacd, STREAM_INPUT_ENDP, read_buf, sizeof(read_buf));
        }
        break;

    case STREAM_OUTPUT_ITF_NUM:
        stream_output_alt_setting = alt_setting;
        if (alt_setting == 0)
        {
            /* Close the output stream endpoint for audio playback */
            uacd_drv_close_stream_endpoint(uacd, STREAM_OUTPUT_ENDP);
        }
        else
        {
            /* Open the output stream endpoint for audio playback */
            uacd_drv_open_stream_endpoint(uacd, STREAM_OUTPUT_ENDP);
            uacd_drv_write(uacd, STREAM_OUTPUT_ENDP, write_buf, 0);
        }
        break;
    }
}

static void uac_get_itf_cb(uacd_handle_t *uacd, uint8_t itf_num, uint8_t *alt_setting)
{
    switch (itf_num)
    {
    case STREAM_INPUT_ITF_NUM:
        *alt_setting = stream_input_alt_setting;
        break;

    case STREAM_OUTPUT_ITF_NUM:
        *alt_setting = stream_output_alt_setting;
        break;
    }
}

static void uac_read_comp_cb(uacd_handle_t *uacd, usb_endp_t endp, void *buf, size_t len)
{
    uint16_t left_buf[sizeof(read_buf) / 4];
    uint16_t right_buf[sizeof(read_buf) / 4];
    uint32_t *src_ptr = (uint32_t *)read_buf;

    /* Convert the incoming 16-bit stereo PCM to 12-bit DAC left and right channel samples */
    size_t frames = len >> 2;
    for (size_t i = 0; i < frames; i++)
    {
        uint32_t frame = *src_ptr++;
        left_buf[i] = ((int16_t)((frame >> 0) & 0xFFFF) + 32768) >> 4;
        right_buf[i] = ((int16_t)((frame >> 16) & 0xFFFF) + 32768) >> 4;
    }
    dac_write(left_buf, right_buf, frames);
    uacd_drv_read(uacd, STREAM_INPUT_ENDP, read_buf, sizeof(read_buf));
}

static void uac_write_comp_cb(uacd_handle_t *uacd, usb_endp_t endp, const void *buf, size_t len)
{
    uint16_t adc_buf[sizeof(write_buf) / 2];
    int16_t *dst_ptr = (int16_t *)write_buf;

    /* Read audio samples from the ADC, convert 12-bit samples to 16-bit PCM for USB transmission */
    size_t adc_len = adc_read(adc_buf, sizeof(adc_buf) / sizeof(adc_buf[0]));
    for (size_t i = 0; i < adc_len; i++)
    {
        dst_ptr[i] = ((adc_buf[i] << 4) - 32768);
    }
    uacd_drv_write(uacd, STREAM_OUTPUT_ENDP, write_buf, adc_len << 1);
}

static void enum_completed_event_cb(usbd_handle_t *h, usbd_event_ctx_t *ctx)
{
    stream_input_alt_setting = 0;
    stream_output_alt_setting = 0;

    /* Initialize UAC device handle */
    memset(&uac_handle, 0, sizeof(uac_handle));
    uac_handle.usbd_handle = h;
    uac_handle.ctrl_itf_num = 0;
    uac_handle.set_itf_cb = uac_set_itf_cb;
    uac_handle.get_itf_cb = uac_get_itf_cb;
    uac_handle.read_comp_cb = uac_read_comp_cb;
    uac_handle.write_comp_cb = uac_write_comp_cb;
    assert(uacd_drv_open(&uac_handle));
    assert(uacd_drv_stream_interface_append(&uac_handle, STREAM_INPUT_ITF_NUM));
    assert(uacd_drv_stream_interface_append(&uac_handle, STREAM_OUTPUT_ITF_NUM));
    assert(uacd_drv_stream_endpoint_append(&uac_handle, STREAM_INPUT_ENDP));
    assert(uacd_drv_stream_endpoint_append(&uac_handle, STREAM_OUTPUT_ENDP));
}

int main(void)
{
    board_init();
    adc_init();
    dac_init();

    usbd_handle = board_usb_init(USBD_INDEX, USB_MODE_DEVICE);
    assert(usbd_handle != NULL);

    assert(usbd_register_event_callback(usbd_handle, USBD_EVENT_ENUM_COMPLETED, enum_completed_event_cb));
    assert(usbd_drv_open(usbd_handle, USB_SPEED_FULL, false, get_stand_desc_cb));

    while (1)
    {
    }

    return 0;
}
