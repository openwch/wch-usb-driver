/********************************** (C) COPYRIGHT *******************************
* File Name          : uach.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB UAC host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @define */
#define USB_LOG_TAG "UACH"

/* @include */
#include "usb_driver.h"
#include "host/usbh_driver_private.h"

#ifdef USB_CLASS_UACH_DRIVER_EN

/* @define */
#ifndef UACH_HANDLE_POOL_SIZE
#define UACH_HANDLE_POOL_SIZE 1
#endif

#ifndef UACH_MAX_STREAM_ITF_COUNT
#define UACH_MAX_STREAM_ITF_COUNT 2
#endif

#define CTRL_XFER_TIMEOUT          100
#define SUPPORTED_PROTOCOL_VERSION 0x0100

/* @enum */
typedef enum
{
    ENUM_STAGE_IDLE,
    ENUM_STAGE_CLOSE_STREAM_ITF,
    ENUM_STAGE_ENUM_FAILED,
    ENUM_STAGE_END,
} enum_stage_t;

typedef enum
{
    OPEN_STAGE_OPEN_STREAM_ITF,
    OPEN_STAGE_SET_SAMPLE_RATE,
    OPEN_STAGE_CLOSE_STREAM_ITF,
    OPEN_STAGE_END,
} open_stage_t;

/* @struct */
typedef struct
{
    bool is_open;
    bool target_state;
    uint8_t stream_dir;
    uint8_t open_stage;
    uint16_t channel_config;
    uint32_t target_sample_rate;

    const usb_desc_interface_t *itf;
    const uac_desc_as_itf_t *stream_itf;
    const uac_desc_as_format_t *format;
    const uac_desc_audio_endpoint_t *ep;
    const uac_desc_audio_as_endpoint_t *as_ep;

    uach_handle_t *uac;
    uach_stream_open_cb open_cb;
    uach_stream_transfer_cb xfer_cb;
} stream_ctx_t;

typedef struct uach_handle
{
    struct uach_handle *next;
    usbh_device_t *dev;

    /* UAC interface information */
    const usb_desc_interface_t *ctrl_itf;
    const uac_desc_header_t *header_desc;

    /* Run-time context */
    uint8_t itf_idx;
    uint8_t enum_stage;
    size_t stream_count;
    stream_ctx_t stream_ctxs[UACH_MAX_STREAM_ITF_COUNT];
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
    __attribute__((aligned(4))) uint8_t ctrl_buf[16];
} uach_handle_t;

/* @global */
static uach_handle_t *uac_list = NULL;
static uach_handle_t handle_pool[UACH_HANDLE_POOL_SIZE] = {0};

/* @function declarations */
static bool uac_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t len);
static bool uac_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num);
static void uac_drv_stream_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);
static void uac_drv_stream_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);

static const usbh_drv_info_t uac_drv_info = {
    .vid = 0x0000,
    .pid = 0x0000,
    .dev_class = 0x00,
    .dev_subclass = 0x00,
    .dev_protocol = 0x00,
    .itf_class = USB_CLASS_AUDIO,
    .itf_subclass = UAC_SUBCLASS_AUDIO_CONTROL,
    .itf_protocol = 0x00,
    .match_bitmap = (USBH_DRV_MATCH_ITF_CLASS | USBH_DRV_MATCH_ITF_SUBCLASS),
    .bind = uac_drv_bind_cb,
    .unbind = uac_drv_unbind_cb,
};

static void enum_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t len)
{
    uach_handle_t *uac = (uach_handle_t *)handle;
    if (!rst)
    {
        USB_LOGE("Handle %p: Enumeration control transfer failed at stage %d", uac, uac->enum_stage);
        uac->enum_stage = ENUM_STAGE_ENUM_FAILED;
        return;
    }

    switch (uac->enum_stage)
    {
    case ENUM_STAGE_CLOSE_STREAM_ITF:
        USB_LOGI("Handle %p: Closed stream interface %d", uac, uac->header_desc->baInterfaceNr[uac->itf_idx]);
        uac->itf_idx++;
        break;
    }
}

static void uac_enum_task(uach_handle_t *uac)
{
    switch (uac->enum_stage)
    {
    case ENUM_STAGE_IDLE:
        uac->itf_idx = 0;
        uac->enum_stage = ENUM_STAGE_CLOSE_STREAM_ITF;
        break;

    case ENUM_STAGE_CLOSE_STREAM_ITF:
        if (usbh_is_ctrl_xfering(uac->dev)) return;
        if (uac->itf_idx < uac->header_desc->bInCollection)
        {
            usb_setup_t *request = &uac->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
            request->bRequest = USB_REQ_SET_INTERFACE;
            request->wValue = 0;
            request->wIndex = uac->header_desc->baInterfaceNr[uac->itf_idx];
            request->wLength = 0;
            usbh_ctrl_xfer(uac->dev, uac, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        }
        else
        {
            uac->enum_stage = ENUM_STAGE_END;
            USB_LOGI("Handle %p: Enumeration completed", uac);
        }
        break;

    case ENUM_STAGE_ENUM_FAILED:
        uac->enum_stage = ENUM_STAGE_END;
        uac_drv_unbind_cb(uac->dev, uac->ctrl_itf->bInterfaceNumber);
        break;
    }
}

static void run_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t len)
{
    stream_ctx_t *stream_ctx = (stream_ctx_t *)handle;

    if (stream_ctx->target_state == false)
    {
        stream_ctx->is_open = false;
        usbh_endp_close(stream_ctx->uac->dev, stream_ctx->ep->bEndpointAddress);
        USB_LOGI("Handle %p: Stream interface %d closed", stream_ctx->uac, stream_ctx->itf->bInterfaceNumber);
    }
    else if (!rst)
    {
        stream_ctx->target_state = false;
        USB_LOGW("Handle %p: Failed to open stream interface %d", stream_ctx->uac, stream_ctx->itf->bInterfaceNumber);
        if (stream_ctx->open_cb)
        {
            stream_ctx->open_cb(stream_ctx, false);
        }
    }
    else if (stream_ctx->open_stage == OPEN_STAGE_END)
    {
        if (stream_ctx->ep)
        {
            rst = usbh_endp_open(stream_ctx->uac->dev, stream_ctx, (const usb_desc_endpoint_t *)stream_ctx->ep,
                                 stream_ctx->stream_dir == UACH_STREAM_DIR_CAPTURE ? uac_drv_stream_read_cb
                                                                                   : uac_drv_stream_write_cb);
        }

        if (rst)
        {
            stream_ctx->is_open = true;
            USB_LOGI("Handle %p: Stream interface %d opened", stream_ctx->uac, stream_ctx->itf->bInterfaceNumber);
        }
        else
        {
            stream_ctx->target_state = false;
            USB_LOGW("Handle %p: Failed to open stream endpoint %02x", stream_ctx->uac,
                     stream_ctx->ep->bEndpointAddress);
        }
        if (stream_ctx->open_cb)
        {
            stream_ctx->open_cb(stream_ctx, rst);
        }
    }
}

static void uac_run_task(uach_handle_t *uac)
{
    for (size_t i = 0; i < uac->stream_count; i++)
    {
        stream_ctx_t *stream_ctx = &uac->stream_ctxs[i];
        if (usbh_is_ctrl_xfering(uac->dev)) return;
        if (stream_ctx->target_state == stream_ctx->is_open) continue;
        switch (stream_ctx->open_stage)
        {
        case OPEN_STAGE_OPEN_STREAM_ITF:
        {
            if (stream_ctx->as_ep && stream_ctx->target_sample_rate &&
                (stream_ctx->as_ep->bmAttributes & UAC_ENDP_SELECT_SAMPLING_FREQ))
            {
                stream_ctx->open_stage = OPEN_STAGE_SET_SAMPLE_RATE;
            }
            else
            {
                stream_ctx->open_stage = OPEN_STAGE_END;
            }
            usb_setup_t *request = &uac->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
            request->bRequest = USB_REQ_SET_INTERFACE;
            request->wValue = stream_ctx->itf->bAlternateSetting;
            request->wIndex = stream_ctx->itf->bInterfaceNumber;
            request->wLength = 0;
            usbh_ctrl_xfer(uac->dev, stream_ctx, request, NULL, CTRL_XFER_TIMEOUT, run_ctrl_xfer_cb);
            break;
        }

        case OPEN_STAGE_SET_SAMPLE_RATE:
        {
            stream_ctx->open_stage = OPEN_STAGE_END;
            usb_setup_t *request = &uac->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_ENDPOINT);
            request->bRequest = UAC_REQ_SET_CUR;
            request->wValue = UAC_ENDP_SELECT_SAMPLING_FREQ << 8;
            request->wIndex = stream_ctx->ep->bEndpointAddress;
            request->wLength = 3;
            memcpy(uac->ctrl_buf, &stream_ctx->target_sample_rate, 3);
            usbh_ctrl_xfer(uac->dev, stream_ctx, request, uac->ctrl_buf, CTRL_XFER_TIMEOUT, run_ctrl_xfer_cb);
            break;
        }

        case OPEN_STAGE_CLOSE_STREAM_ITF:
        {
            stream_ctx->open_stage = OPEN_STAGE_END;
            usb_setup_t *request = &uac->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
            request->bRequest = USB_REQ_SET_INTERFACE;
            request->wValue = 0;
            request->wIndex = stream_ctx->itf->bInterfaceNumber;
            request->wLength = 0;
            usbh_ctrl_xfer(uac->dev, stream_ctx, request, NULL, CTRL_XFER_TIMEOUT, run_ctrl_xfer_cb);
            break;
        }
        case OPEN_STAGE_END:
        {
            /* Never reached during normal operation */
            stream_ctx->target_state = stream_ctx->is_open;
            break;
        }
        }
    }
}

void uach_drv_task(void)
{
    uach_handle_t *uac = uac_list;
    while (uac)
    {
        uach_handle_t *next = uac->next;
        if (!uac->dev->suspended)
        {
            if (uac->enum_stage < ENUM_STAGE_END)
            {
                uac_enum_task(uac);
            }
            else
            {
                uac_run_task(uac);
            }
        }
        uac = next;
    }
}

bool uach_drv_open(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_register_driver(h, &uac_drv_info);
}

bool uach_drv_close(usbh_handle_t *h)
{
    if (!h) return false;
    return usbh_unregister_driver(h, &uac_drv_info);
}

size_t uach_get_count(void)
{
    size_t count = 0;
    uach_handle_t *uac = uac_list;
    while (uac)
    {
        if (uac->enum_stage >= ENUM_STAGE_END) count++;
        uac = uac->next;
    }
    return count;
}

uach_handle_t *uach_get_handle(size_t index)
{
    size_t i = 0;
    uach_handle_t *uac = uac_list;
    while (uac)
    {
        if (uac->enum_stage >= ENUM_STAGE_END)
        {
            if (i == index) return uac;
            i++;
        }
        uac = uac->next;
    }
    return NULL;
}

size_t uach_get_stream_info(uach_handle_t *uac, uach_stream_info_t *info, size_t max_count)
{
    if (!uac || !info || !max_count) return 0;

    size_t count = 0;
    for (size_t i = 0; i < USB_MIN(max_count, uac->stream_count); i++)
    {
        stream_ctx_t *stream_ctx = &uac->stream_ctxs[i];
        uach_stream_info_t *stream_info = &info[count];
        if (!stream_ctx->stream_itf || !stream_ctx->format) continue;
        memset(stream_info, 0, sizeof(uach_stream_info_t));
        stream_info->dir = stream_ctx->stream_dir;
        stream_info->channel_config = stream_ctx->channel_config;
        stream_info->delay = stream_ctx->stream_itf->bDelay;
        stream_info->format_tag = stream_ctx->stream_itf->wFormatTag;
        stream_info->format_type = stream_ctx->format->bFormatType;
        stream_info->channels = stream_ctx->format->bNrChannels;
        stream_info->sub_frame_size = stream_ctx->format->bSubframeSize;
        stream_info->bit_resolution = stream_ctx->format->bBitResolution;
        stream_info->sample_freq_type = stream_ctx->format->bSamFreqType;
        if (stream_ctx->format->bSamFreqType == 0)
        {
            if (stream_ctx->format->bLength - sizeof(uac_desc_as_format_t) == 6)
            {
                memcpy(&stream_info->min_sample_freq, &stream_ctx->format->tSamFreq[0], 3);
                memcpy(&stream_info->max_sample_freq, &stream_ctx->format->tSamFreq[3], 3);
            }
        }
        else
        {
            size_t sample_count = USB_MIN((stream_ctx->format->bLength - sizeof(uac_desc_as_format_t)) / 3,
                                          USB_ARRAY_SIZE(stream_info->sample_freq));
            for (size_t j = 0; j < sample_count; j++)
            {
                memcpy(&stream_info->sample_freq[j], &stream_ctx->format->tSamFreq[j * 3], 3);
            }
        }
        stream_info->stream_handle = stream_ctx;
        count++;
    }
    return count;
}

void uach_stream_open(void *handle, uint32_t sample_rate, uach_stream_open_cb cb)
{
    if (!handle)
    {
        if (cb) cb(handle, false);
        return;
    }
    stream_ctx_t *stream_ctx = (stream_ctx_t *)handle;
    stream_ctx->open_cb = cb;
    stream_ctx->target_state = true;
    stream_ctx->target_sample_rate = sample_rate;
    stream_ctx->open_stage = OPEN_STAGE_OPEN_STREAM_ITF;
}

void uach_stream_close(void *handle)
{
    if (!handle) return;
    stream_ctx_t *stream_ctx = (stream_ctx_t *)handle;
    stream_ctx->target_state = false;
    stream_ctx->open_stage = OPEN_STAGE_CLOSE_STREAM_ITF;
}

bool uach_stream_is_open(void *handle)
{
    if (!handle) return false;
    stream_ctx_t *stream_ctx = (stream_ctx_t *)handle;
    return stream_ctx->is_open;
}

bool uach_stream_capture(void *handle, void *buf, size_t size, uach_stream_transfer_cb cb)
{
    if (!handle || !buf || !size) return false;
    stream_ctx_t *stream_ctx = (stream_ctx_t *)handle;
    if (!stream_ctx->is_open || stream_ctx->stream_dir != UACH_STREAM_DIR_CAPTURE) return false;
    stream_ctx->xfer_cb = cb;
    return usbh_endp_read(stream_ctx->uac->dev, stream_ctx->ep->bEndpointAddress, buf, size, 0);
}

bool uach_stream_playback(void *handle, void *buf, size_t size, uach_stream_transfer_cb cb)
{
    if (!handle || !buf || !size) return false;
    stream_ctx_t *stream_ctx = (stream_ctx_t *)handle;
    if (!stream_ctx->is_open || stream_ctx->stream_dir != UACH_STREAM_DIR_PLAYBACK) return false;
    stream_ctx->xfer_cb = cb;
    return usbh_endp_write(stream_ctx->uac->dev, stream_ctx->ep->bEndpointAddress, buf, size, 0);
}

static bool element_array_parse(const void **element_desc_array, const uac_desc_header_t *header_desc)
{
    const uint8_t *curr = (const uint8_t *)header_desc + header_desc->bLength;
    const uint8_t *end = (const uint8_t *)header_desc + header_desc->wTotalLength;

    while (curr < end)
    {
        uint8_t len = curr[0];
        uint8_t type = curr[1];
        uint8_t sub_type = curr[2];
        if (type != USB_DESC_CS_INTERFACE) return false;

        switch (sub_type)
        {
        case UAC_AC_DESC_INPUT_TERMINAL:
        {
            const uac_desc_input_terminal_t *input_terminal = (const uac_desc_input_terminal_t *)curr;
            if (element_desc_array[input_terminal->bTerminalID]) return false;
            element_desc_array[input_terminal->bTerminalID] = input_terminal;
            break;
        }
        case UAC_AC_DESC_OUTPUT_TERMINAL:
        {
            const uac_desc_output_terminal_t *output_terminal = (const uac_desc_output_terminal_t *)curr;
            if (element_desc_array[output_terminal->bTerminalID]) return false;
            element_desc_array[output_terminal->bTerminalID] = output_terminal;
            break;
        }
        case UAC_AC_DESC_MIXER_UNIT:
        {
            const uac_desc_mixer_unit_t *mixer_unit = (const uac_desc_mixer_unit_t *)curr;
            if (element_desc_array[mixer_unit->bUnitID]) return false;
            element_desc_array[mixer_unit->bUnitID] = mixer_unit;
            break;
        }
        case UAC_AC_DESC_SELECTOR_UNIT:
        {
            const uac_desc_select_unit_t *select_unit = (const uac_desc_select_unit_t *)curr;
            if (element_desc_array[select_unit->bUnitID]) return false;
            element_desc_array[select_unit->bUnitID] = select_unit;
            break;
        }
        case UAC_AC_DESC_FEATURE_UNIT:
        {
            const uac_desc_feature_unit_t *feature_unit = (const uac_desc_feature_unit_t *)curr;
            if (element_desc_array[feature_unit->bUnitID]) return false;
            element_desc_array[feature_unit->bUnitID] = feature_unit;
            break;
        }
        default:
            return false;
        }
        curr += len;
    }
    return true;
}

static bool element_desc_check(const void **array, const void *element, uint8_t *ch, uint16_t *ch_config, uint8_t depth)
{
    if (!element || depth > 12) return false;

    const uint8_t *desc = (const uint8_t *)element;
    switch (desc[2])
    {
    case UAC_AC_DESC_INPUT_TERMINAL:
    {
        const uac_desc_input_terminal_t *input_terminal = (const uac_desc_input_terminal_t *)desc;
        *ch = input_terminal->bNrChannels;
        *ch_config = input_terminal->wChannelConfig;
        return true;
    }
    case UAC_AC_DESC_OUTPUT_TERMINAL:
    {
        const uac_desc_output_terminal_t *output_terminal = (const uac_desc_output_terminal_t *)desc;
        return element_desc_check(array, array[output_terminal->bSourceID], ch, ch_config, depth + 1);
    }
    case UAC_AC_DESC_MIXER_UNIT:
    {
        const uac_desc_mixer_unit_t *mixer_unit = (const uac_desc_mixer_unit_t *)desc;
        if (!mixer_unit->bNrInPins) return false;
        for (size_t i = 0; i < mixer_unit->bNrInPins; i++)
        {
            if (!element_desc_check(array, array[desc[5 + i]], ch, ch_config, depth + 1)) return false;
        }
        *ch = ((const uint8_t *)mixer_unit)[5 + mixer_unit->bNrInPins];
        *ch_config = ((const uint8_t *)mixer_unit)[6 + mixer_unit->bNrInPins];
        return true;
    }
    case UAC_AC_DESC_SELECTOR_UNIT:
    {
        const uac_desc_select_unit_t *select_unit = (const uac_desc_select_unit_t *)desc;
        if (!select_unit->bNrInPins) return false;
        for (size_t i = 0; i < select_unit->bNrInPins; i++)
        {
            if (!element_desc_check(array, array[desc[5 + i]], ch, ch_config, depth + 1)) return false;
        }
        return true;
    }
    case UAC_AC_DESC_FEATURE_UNIT:
    {
        const uac_desc_feature_unit_t *feature_unit = (const uac_desc_feature_unit_t *)desc;
        if (!element_desc_check(array, array[feature_unit->bSourceID], ch, ch_config, depth + 1)) return false;
        uint8_t feature_unit_length = sizeof(uac_desc_feature_unit_t) + ((*ch) + 1) * feature_unit->bControlSize;
        if (feature_unit->bLength != feature_unit_length) return false;
        return true;
    }
    }
    return false;
}

static bool topology_structure_check(const void **element_desc_array)
{
    for (size_t i = 0; i < 256; i++)
    {
        if (element_desc_array[i] && ((const uint8_t *)element_desc_array[i])[2] == UAC_AC_DESC_OUTPUT_TERMINAL)
        {
            uint8_t channels = 0;
            uint16_t channel_config = 0;
            if (!element_desc_check(element_desc_array, element_desc_array[i], &channels, &channel_config, 0))
            {
                return false;
            }
        }
    }
    return true;
}

static uach_handle_t *handle_alloc(void)
{
    for (size_t i = 0; i < USB_ARRAY_SIZE(handle_pool); i++)
    {
        uach_handle_t *uac = &handle_pool[i];
        if (uac->dev == NULL)
        {
            memset(uac, 0, sizeof(uach_handle_t));
            return uac;
        }
    }
    return NULL;
}

static void stream_itf_parse(uach_handle_t *uac, const void **element_desc_array)
{
    uac->stream_count = 0;
    stream_ctx_t *stream = NULL;
    uint32_t stream_itf_bitmap[8] = {0};
    const uac_desc_audio_endpoint_t *ep = NULL;

    for (size_t i = 0; i < uac->header_desc->bInCollection; i++)
    {
        uint8_t itf_num = uac->header_desc->baInterfaceNr[i];
        stream_itf_bitmap[itf_num >> 5] |= (1U << (itf_num & 0x1F));
    }

    const uint8_t *curr = (const uint8_t *)uac->dev->config_desc;
    const uint8_t *end = curr + ((const usb_desc_config_t *)uac->dev->config_desc)->wTotalLength;
    while (curr < end)
    {
        uint8_t len = curr[0];
        uint8_t type = curr[1];
        switch (type)
        {
        case USB_DESC_INTERFACE:
        {
            const usb_desc_interface_t *itf = (const usb_desc_interface_t *)curr;
            if (stream_itf_bitmap[itf->bInterfaceNumber >> 5] & (1U << (itf->bInterfaceNumber & 0x1F)) &&
                itf->bAlternateSetting > 0 && uac->stream_count < USB_ARRAY_SIZE(uac->stream_ctxs))
            {
                stream = &uac->stream_ctxs[uac->stream_count++];
                stream->itf = itf;
                stream->uac = uac;
                ep = NULL;
            }
            else
            {
                stream = NULL;
            }
            break;
        }

        case USB_DESC_ENDPOINT:
        {
            if (stream && ep == NULL)
            {
                ep = (const uac_desc_audio_endpoint_t *)curr;
                stream->ep = ep;
            }
            break;
        }

        case USB_DESC_CS_INTERFACE:
        {
            if (!stream) break;
            uint8_t sub_type = curr[2];
            if (sub_type == UAC_AS_DESC_GENERAL)
            {
                stream->stream_itf = (const uac_desc_as_itf_t *)curr;
            }
            else if (sub_type == UAC_AS_DESC_FORMAT_TYPE)
            {
                stream->format = (const uac_desc_as_format_t *)curr;
            }
            break;
        }

        case USB_DESC_CS_ENDPOINT:
            if (stream)
            {
                stream->as_ep = (const uac_desc_audio_as_endpoint_t *)curr;
            }
            break;
        }

        curr += len;
    }

    for (int i = 0; i < uac->stream_count; i++)
    {
        stream = &uac->stream_ctxs[i];
        if (!stream->stream_itf) continue;

        const void *term_desc = element_desc_array[stream->stream_itf->bTerminalLink];
        if (!term_desc) continue;

        uint8_t channels = 0;
        uint16_t channel_config = 0;
        if (element_desc_check(element_desc_array, term_desc, &channels, &channel_config, 0))
        {
            stream->channel_config = channel_config;
        }

        uint8_t term_type = ((const uint8_t *)term_desc)[2];
        if (term_type == UAC_AC_DESC_INPUT_TERMINAL)
        {
            stream->stream_dir = UACH_STREAM_DIR_PLAYBACK;
        }
        else if (term_type == UAC_AC_DESC_OUTPUT_TERMINAL)
        {
            stream->stream_dir = UACH_STREAM_DIR_CAPTURE;
        }
    }
}

static bool uac_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t len)
{
    if (!dev || dev->speed == USB_SPEED_LOW || !desc || len == 0) return false;

    const void *element_desc_array[256] = {0};
    const usb_desc_interface_t *ctrl_itf = NULL;
    const uac_desc_header_t *header_desc = NULL;

    const usb_desc_interface_t *curr_itf = NULL;
    const uint8_t *curr = (const uint8_t *)desc;
    const uint8_t *end = curr + len;

    while (curr < end)
    {
        uint8_t len = curr[0];
        uint8_t type = curr[1];

        if (type == USB_DESC_INTERFACE)
        {
            curr_itf = (const usb_desc_interface_t *)curr;
            if (curr_itf->bInterfaceSubClass == UAC_SUBCLASS_AUDIO_CONTROL)
            {
                ctrl_itf = curr_itf;
            }
        }
        else if (type == USB_DESC_CS_INTERFACE)
        {
            uint8_t sub_type = curr[2];
            if (curr_itf && curr_itf->bInterfaceSubClass == UAC_SUBCLASS_AUDIO_CONTROL &&
                sub_type == UAC_AC_DESC_HEADER)
            {
                if (header_desc) return false;
                header_desc = (const uac_desc_header_t *)curr;
                if (header_desc->bLength < sizeof(uac_desc_header_t)) return false;
                if (header_desc->bcdADC > SUPPORTED_PROTOCOL_VERSION) return false;
                if (!header_desc->bInCollection) return false;
                if (!element_array_parse(element_desc_array, header_desc)) return false;
                if (!topology_structure_check(element_desc_array)) return false;
            }
            break;
        }
        curr += len;
    }

    if (!ctrl_itf || !header_desc) return false;

    /* Allocate and initialize a new uac handle */
    uach_handle_t *uac = handle_alloc();
    if (!uac)
    {
        USB_LOGE("Handle %p: Failed to allocate uac handle", dev);
        return false;
    }

    uac->dev = dev;
    uac->ctrl_itf = ctrl_itf;
    uac->header_desc = header_desc;
    stream_itf_parse(uac, element_desc_array);
    usbh_list_append((void **)&uac_list, uac);
    return true;
}

static bool uac_drv_unbind_cb(usbh_device_t *dev, uint8_t itf_num)
{
    uach_handle_t *uac = uac_list;
    while (uac)
    {
        uach_handle_t *next = uac->next;
        if (uac->dev == dev && (itf_num == USBH_DRV_ALL_INTERFACE || uac->ctrl_itf->bInterfaceNumber == itf_num))
        {
            for (size_t i = 0; i < uac->stream_count; i++)
            {
                stream_ctx_t *stream_ctx = &uac->stream_ctxs[i];
                if (stream_ctx->ep) usbh_endp_close(uac->dev, stream_ctx->ep->bEndpointAddress);
            }
            usbh_list_remove((void **)&uac_list, uac);
            memset(uac, 0, sizeof(uach_handle_t));
        }
        uac = next;
    }
    return true;
}

static void uac_drv_stream_read_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    stream_ctx_t *stream_ctx = (stream_ctx_t *)handle;
    if (!stream_ctx || !stream_ctx->is_open) return;
    if (stream_ctx->xfer_cb)
    {
        stream_ctx->xfer_cb(stream_ctx, (void *)buf, len);
    }
}

static void uac_drv_stream_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    stream_ctx_t *stream_ctx = (stream_ctx_t *)handle;
    if (!stream_ctx || !stream_ctx->is_open) return;
    if (stream_ctx->xfer_cb)
    {
        stream_ctx->xfer_cb(stream_ctx, (void *)buf, len);
    }
}

#endif // USB_CLASS_UACH_DRIVER_EN
