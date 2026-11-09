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

#define LOOPBACK_BUF_SIZE 1024

/* @enum */
typedef enum
{
    STREAM_STATE_IDLE,
    STREAM_STATE_SET_ALT,
    STREAM_STATE_SET_RATE,
    STREAM_STATE_RUNNING,
} stream_state_t;

/* @struct */
typedef struct
{
    uach_stream_handle_t *handle;
    uach_stream_alt_info_t alt;
    uint32_t sample_rate;
    uint8_t state;
} loopback_stream_t;

/* @global */
static usbh_handle_t *usbh_handle;
static uach_handle_t *active_uac;

static loopback_stream_t capture_stream;
static loopback_stream_t playback_stream;

static size_t playback_size[2];
static uint8_t playback_busy_index;
static uint8_t playback_pending_index;
static bool playback_busy;
static bool playback_pending;
static bool capture_started;
static __attribute__((aligned(4))) uint8_t capture_buf[LOOPBACK_BUF_SIZE];
static __attribute__((aligned(4))) uint8_t playback_buf[2][LOOPBACK_BUF_SIZE];

/* @function declarations */
static void capture_xfer_cb(uach_stream_handle_t *stream, void *buf, size_t len);
static void playback_xfer_cb(uach_stream_handle_t *stream, void *buf, size_t len);

static bool alt_is_pcm16(const uach_stream_alt_info_t *alt)
{
    if (alt->format_type != UAC_FORMAT_TYPE_I) return false;
    if (alt->format_tag != UAC_FORMAT_TAG_TYPE_I_PCM) return false;
    if (alt->sub_frame_size != 2 || alt->bit_resolution != 16) return false;
    if (alt->channels != 1 && alt->channels != 2) return false;
    if (alt->max_packet_size == 0 || alt->max_packet_size > LOOPBACK_BUF_SIZE) return false;
    return true;
}

static bool alt_rate_supported(const uach_stream_alt_info_t *alt, uint32_t sample_rate)
{
    if (alt->sample_freq_type == 0)
    {
        if (sample_rate < alt->min_sample_freq || sample_rate > alt->max_sample_freq) return false;
        return alt->freq_ctrl || sample_rate == alt->min_sample_freq;
    }

    /* The device uses the first advertised rate when the endpoint has no frequency control */
    if (!alt->freq_ctrl && sample_rate != alt->sample_freq[0]) return false;

    for (size_t i = 0; i < alt->sample_freq_count; i++)
    {
        if (alt->sample_freq[i] == sample_rate) return true;
    }
    return false;
}

static uint32_t alt_first_rate(const uach_stream_alt_info_t *alt)
{
    if (alt->sample_freq_type == 0)
    {
        if (alt->freq_ctrl) return USB_MAX(alt->min_sample_freq, USB_MIN(alt->max_sample_freq, 48000));
        return alt->min_sample_freq;
    }
    return alt->sample_freq_count ? alt->sample_freq[0] : 0;
}

static bool stream_find_alt(uach_stream_handle_t *stream, uint32_t sample_rate, uach_stream_alt_info_t *alt_info)
{
    uach_stream_info_t info;

    if (!stream || !uach_stream_get_info(stream, &info)) return false;

    for (size_t i = 0; i < info.alt_count; i++)
    {
        uach_stream_alt_info_t alt;

        if (!uach_stream_get_alt_info(stream, i, &alt)) continue;
        if (!alt_is_pcm16(&alt)) continue;
        if (sample_rate && !alt_rate_supported(&alt, sample_rate)) continue;

        *alt_info = alt;
        return true;
    }
    return false;
}

static uach_stream_handle_t *find_stream(uach_handle_t *uac, uint8_t dir)
{
    size_t count = uach_get_stream_count(uac);

    for (size_t i = 0; i < count; i++)
    {
        uach_stream_handle_t *stream = uach_get_stream_handle(uac, i);
        uach_stream_info_t info;
        uach_stream_alt_info_t alt;

        if (!stream || !uach_stream_get_info(stream, &info)) continue;
        if (info.dir != dir) continue;
        if (stream_find_alt(stream, 0, &alt)) return stream;
    }
    return NULL;
}

static loopback_stream_t *stream_get_ctx(uach_stream_handle_t *stream)
{
    if (capture_stream.handle == stream) return &capture_stream;
    if (playback_stream.handle == stream) return &playback_stream;
    return NULL;
}

static size_t capture_to_playback(const uint8_t *src, size_t len, uint8_t *dst, size_t size)
{
    const uach_stream_alt_info_t *cap_alt = &capture_stream.alt;
    const uach_stream_alt_info_t *play_alt = &playback_stream.alt;
    size_t cap_frame_size = cap_alt->channels * cap_alt->sub_frame_size;
    size_t play_frame_size = play_alt->channels * play_alt->sub_frame_size;
    size_t frames = len / cap_frame_size;
    size_t max_frames = USB_MIN(size, play_alt->max_packet_size) / play_frame_size;

    if (frames > max_frames) frames = max_frames;
    if (!frames) return 0;

    if (cap_alt->channels == play_alt->channels)
    {
        memcpy(dst, src, frames * play_frame_size);
    }
    else if (cap_alt->channels == 1)
    {
        /* Mono capture to stereo playback */
        const int16_t *src16 = (const int16_t *)src;
        int16_t *dst16 = (int16_t *)dst;

        for (size_t i = 0; i < frames; i++)
        {
            dst16[i * 2 + 0] = src16[i];
            dst16[i * 2 + 1] = src16[i];
        }
    }
    else
    {
        /* Stereo capture to mono playback */
        const int16_t *src16 = (const int16_t *)src;
        int16_t *dst16 = (int16_t *)dst;

        for (size_t i = 0; i < frames; i++)
        {
            dst16[i] = (int16_t)(((int32_t)src16[i * 2 + 0] + src16[i * 2 + 1]) / 2);
        }
    }

    return frames * play_frame_size;
}

static bool playback_submit(uint8_t index)
{
    if (!playback_stream.handle) return false;
    if (!uach_stream_playback(playback_stream.handle, playback_buf[index], playback_size[index], playback_xfer_cb))
        return false;

    playback_busy = true;
    playback_busy_index = index;
    return true;
}

static void playback_xfer_cb(uach_stream_handle_t *stream, void *buf, size_t len)
{
    playback_busy = false;

    /* Send the packet buffered while this transfer was in flight */
    if (playback_pending)
    {
        playback_pending = false;
        playback_submit(playback_pending_index);
    }
}

static void capture_xfer_cb(uach_stream_handle_t *stream, void *buf, size_t len)
{
    if (playback_stream.state != STREAM_STATE_RUNNING)
    {
        /* Keep capturing even when the playback stream is not available */
        if (!uach_stream_capture(stream, capture_buf, sizeof(capture_buf), capture_xfer_cb))
        {
            capture_stream.state = STREAM_STATE_IDLE;
        }
        return;
    }

    uint8_t index = 0xFF;
    if (!playback_busy)
    {
        playback_pending = false;
        index = 0;
    }
    else if (!playback_pending)
    {
        /* Buffer the packet in the slot that is not being sent */
        index = playback_busy_index ^ 1;
    }

    if (index != 0xFF)
    {
        size_t size = capture_to_playback(buf, len, playback_buf[index], sizeof(playback_buf[index]));

        if (size)
        {
            playback_size[index] = size;

            if (!playback_busy)
            {
                playback_submit(index);
            }
            else
            {
                playback_pending = true;
                playback_pending_index = index;
            }
        }
    }

    if (!uach_stream_capture(stream, capture_buf, sizeof(capture_buf), capture_xfer_cb))
    {
        capture_stream.state = STREAM_STATE_IDLE;
    }
}

static void stream_ctrl_cb(uach_stream_handle_t *stream, bool rst)
{
    loopback_stream_t *s = stream_get_ctx(stream);
    const char *name;

    if (!s) return;
    name = s == &capture_stream ? "Capture" : "Playback";

    if (!rst)
    {
        printf("UAC: %s stream configuration failed\n", name);
        s->state = STREAM_STATE_IDLE;
        return;
    }

    if (s->state == STREAM_STATE_SET_ALT && s->alt.freq_ctrl)
    {
        s->state = STREAM_STATE_SET_RATE;
        if (!uach_stream_set_sample_rate(stream, s->sample_rate, stream_ctrl_cb))
        {
            printf("UAC: %s stream sample rate request rejected\n", name);
            s->state = STREAM_STATE_IDLE;
        }
        return;
    }

    s->state = STREAM_STATE_RUNNING;

    uach_stream_info_t info;
    if (uach_stream_get_info(stream, &info))
    {
        printf("UAC: %s stream itf %u alt %u ready (%u Hz, %u ch, %u-byte packets)\n", name, info.itf_num,
               s->alt.alt_setting, (unsigned int)s->sample_rate, s->alt.channels, s->alt.max_packet_size);
    }

    if (capture_started) return;
    if (capture_stream.state != STREAM_STATE_RUNNING || playback_stream.state != STREAM_STATE_RUNNING) return;

    if (!uach_stream_capture(capture_stream.handle, capture_buf, sizeof(capture_buf), capture_xfer_cb))
    {
        printf("UAC: Failed to start capture\n");
        capture_stream.state = STREAM_STATE_IDLE;
        return;
    }

    capture_started = true;
    printf("UAC: Audio loopback started\n");
}

static void loopback_reset(void)
{
    memset(&capture_stream, 0, sizeof(capture_stream));
    memset(&playback_stream, 0, sizeof(playback_stream));
    memset(playback_size, 0, sizeof(playback_size));
    playback_busy_index = 0;
    playback_pending_index = 0;
    playback_busy = false;
    playback_pending = false;
    capture_started = false;
}

static bool loopback_configure(uach_handle_t *uac)
{
    static const uint32_t preferred_rate_list[] = {48000, 44100, 32000, 16000};
    uach_stream_handle_t *cap_stream = find_stream(uac, UACH_STREAM_DIR_CAPTURE);
    uach_stream_handle_t *play_stream = find_stream(uac, UACH_STREAM_DIR_PLAYBACK);
    uach_stream_alt_info_t cap_alt;
    uach_stream_alt_info_t play_alt;
    uint32_t sample_rate = 0;

    if (!cap_stream || !play_stream)
    {
        printf("UAC: No %s stream with 16-bit PCM format\n", cap_stream ? "playback" : "capture");
        return false;
    }

    for (size_t i = 0; i < USB_ARRAY_SIZE(preferred_rate_list); i++)
    {
        uint32_t rate = preferred_rate_list[i];

        if (stream_find_alt(cap_stream, rate, &cap_alt) && stream_find_alt(play_stream, rate, &play_alt))
        {
            sample_rate = rate;
            break;
        }
    }

    if (!sample_rate)
    {
        uach_stream_alt_info_t alt;
        uint32_t rate;
        bool found = false;

        /* Fall back to the first sample rate advertised by either stream */
        if (stream_find_alt(cap_stream, 0, &alt))
        {
            rate = alt_first_rate(&alt);
            found = rate && stream_find_alt(play_stream, rate, &play_alt) &&
                    stream_find_alt(cap_stream, rate, &cap_alt);
        }
        if (!found && stream_find_alt(play_stream, 0, &alt))
        {
            rate = alt_first_rate(&alt);
            found = rate && stream_find_alt(cap_stream, rate, &cap_alt) &&
                    stream_find_alt(play_stream, rate, &play_alt);
        }
        if (found) sample_rate = rate;
    }

    if (!sample_rate)
    {
        printf("UAC: No common 16-bit PCM sample rate between capture and playback streams\n");
        return false;
    }

    capture_stream.handle = cap_stream;
    capture_stream.alt = cap_alt;
    capture_stream.sample_rate = sample_rate;
    capture_stream.state = STREAM_STATE_SET_ALT;

    playback_stream.handle = play_stream;
    playback_stream.alt = play_alt;
    playback_stream.sample_rate = sample_rate;
    playback_stream.state = STREAM_STATE_SET_ALT;

    printf("UAC: Loopback format %u Hz, %u-bit PCM, capture %u ch, playback %u ch\n", (unsigned int)sample_rate,
           cap_alt.bit_resolution, cap_alt.channels, play_alt.channels);

    if (!uach_stream_set_alt_setting(cap_stream, cap_alt.alt_setting, stream_ctrl_cb) ||
        !uach_stream_set_alt_setting(play_stream, play_alt.alt_setting, stream_ctrl_cb))
    {
        printf("UAC: Failed to request stream alternate setting\n");
        loopback_reset();
        return false;
    }

    return true;
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
        }

        /* Restart the loopback if the active device was reset by a re-enumeration */
        if (capture_started)
        {
            uach_stream_info_t info;
            if (!uach_stream_get_info(capture_stream.handle, &info) || info.cur_alt == 0)
            {
                loopback_reset();
                active_uac = NULL;
            }
        }

        /* Configure the audio loopback for the first UAC device */
        uach_handle_t *uac = uach_get_handle(0);
        if (uac != active_uac)
        {
            loopback_reset();
            active_uac = uac;

            if (uac) loopback_configure(uac);
        }
    }

    return 0;
}
