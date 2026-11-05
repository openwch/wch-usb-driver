/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : Main program for UAC headset example
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include*/
#include <assert.h>
#include <string.h>

#include "board.h"

#include "usb_driver.h"

/* @define */
#ifndef USBH_INDEX
#define USBH_INDEX 0
#endif

#if USBH_INDEX >= USB_COUNT
#error "Not supported: USBH_INDEX exceeds USB_COUNT"
#endif

/* @global */
static usbh_handle_t *usbh_handle;
static size_t playback_size;
static void *capture_stream_handle;
static void *playback_stream_handle;
static __attribute__((aligned(4))) uint8_t capture_buf[1024];
static __attribute__((aligned(4))) uint8_t playback_buf[1024];

static void capture_stream_transfer_cb(void *stream_handle, void *buf, size_t size)
{
    uach_stream_capture(stream_handle, capture_buf, sizeof(capture_buf), capture_stream_transfer_cb);
}

static void playback_stream_transfer_cb(void *stream_handle, void *buf, size_t size)
{
    uach_stream_playback(stream_handle, playback_buf, playback_size, playback_stream_transfer_cb);
}

static void capture_stream_open_cb(void *stream_handle, bool rst)
{
    if (rst)
    {
        uach_stream_capture(stream_handle, capture_buf, sizeof(capture_buf), capture_stream_transfer_cb);
    }
}

static void playback_stream_open_cb(void *stream_handle, bool rst)
{
    if (rst)
    {
        uach_stream_playback(playback_stream_handle, playback_buf, playback_size, playback_stream_transfer_cb);
    }
}

int main(void)
{
    board_init();

    usbh_handle = (usbh_handle_t *)board_usb_init(USBH_INDEX, USB_MODE_HOST);
    assert(usbh_handle != NULL);

    assert(usbh_drv_open(usbh_handle));
    assert(hubh_drv_open(usbh_handle));
    assert(uach_drv_open(usbh_handle));

    while (1)
    {
        usbh_drv_task(usbh_handle);
        hubh_drv_task();
        uach_drv_task();

        /* Check for changes in the number of connected UAC devices */
        static size_t last_uac_count = 0;
        size_t curr_uac_count = uach_get_count();
        if (curr_uac_count != last_uac_count)
        {
            printf("UAC device count changed: %u -> %u\n", last_uac_count, curr_uac_count);
            last_uac_count = curr_uac_count;
            capture_stream_handle = NULL;
            playback_stream_handle = NULL;
        }

        /* Get the handle for the first UAC device */
        uach_handle_t *uac = uach_get_handle(0);

        /* Skip the rest of the loop if no UAC device is connected */
        if (!uac) continue;

        /* Get stream information for the connected UAC device */
        uach_stream_info_t stream_info[6];
        size_t count = uach_get_stream_info(uac, stream_info, sizeof(stream_info) / sizeof(stream_info[0]));
        for (size_t i = 0; i < count; i++)
        {
            uach_stream_info_t *info = &stream_info[i];
            uint32_t sample_freq = info->sample_freq[0];

            /* Open the capture or playback stream if not already opened */
            if (info->dir == UACH_STREAM_DIR_CAPTURE && !capture_stream_handle)
            {
                capture_stream_handle = info->stream_handle;
                uach_stream_open(capture_stream_handle, sample_freq, capture_stream_open_cb);
            }

            /* Open the playback stream if not already opened */
            if (info->dir == UACH_STREAM_DIR_PLAYBACK && !playback_stream_handle)
            {
                playback_size = info->channels * info->sub_frame_size * sample_freq / 1000;
                playback_stream_handle = info->stream_handle;
                uach_stream_open(playback_stream_handle, sample_freq, playback_stream_open_cb);
            }
        }
    }

    return 0;
}
