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

#ifndef UACH_MAX_STREAM_ALT_SETTING_COUNT
#define UACH_MAX_STREAM_ALT_SETTING_COUNT 2
#endif

#define CTRL_XFER_TIMEOUT          100
#define SUPPORTED_PROTOCOL_VERSION 0x0100
#define ELEMENT_MAX_DEPTH          16
#define ELEMENT_MAX_COUNT          256

/* @enum */
typedef enum
{
    ENUM_STAGE_IDLE,
    ENUM_STAGE_CLOSE_INTERFACE,
    ENUM_STAGE_ENUM_FAILED,
    ENUM_STAGE_END,
} enum_stage_t;

typedef enum
{
    REQ_STAGE_IDLE,
    REQ_STAGE_SET_ALT,
    REQ_STAGE_SET_ALT_BUSY,
    REQ_STAGE_SET_RATE,
    REQ_STAGE_SET_RATE_BUSY,
} req_stage_t;

/* @struct */
typedef struct
{
    const usb_desc_interface_t *itf;
    const uac_desc_as_itf_t *general;
    const uac_desc_as_format_t *format;
    const uac_desc_audio_endpoint_t *ep;
    const uac_desc_audio_as_endpoint_t *as_ep;
    const uac_desc_audio_endpoint_t *sync_ep;
} uach_stream_alt_setting_t;

typedef struct uach_stream_handle
{
    uach_handle_t *uac;

    /* Stream interface information */
    uint8_t itf_num;
    uint8_t dir;
    uint8_t terminal_link;
    uint8_t alt_count;
    uint16_t channel_config;
    uach_stream_alt_setting_t alt_settings[UACH_MAX_STREAM_ALT_SETTING_COUNT];

    /* Run-time context */
    uint8_t req_stage;
    uint8_t req_alt;
    bool req_rollback;
    uint32_t req_rate;
    uint32_t cur_sample_rate;
    const uach_stream_alt_setting_t *cur_alt;
    uach_stream_ctrl_cb ctrl_cb;
    uach_stream_xfer_cb xfer_cb;
} uach_stream_handle_t;

typedef struct uach_handle
{
    struct uach_handle *next;
    usbh_device_t *dev;

    /* UAC interface information */
    const usb_desc_interface_t *ctrl_itf;

    /* Run-time context */
    uint8_t enum_stage;
    uint8_t cur_stream;
    uint8_t stream_count;
    uach_stream_handle_t streams[UACH_MAX_STREAM_ITF_COUNT];
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
    __attribute__((aligned(4))) uint8_t ctrl_buf[4];
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
    case ENUM_STAGE_CLOSE_INTERFACE:
    {
        USB_LOGI("Handle %p: Closing interface %d", uac, uac->streams[uac->cur_stream].itf_num);
        uac->cur_stream++;
        if (uac->cur_stream < uac->stream_count)
        {
            usb_setup_t *request = &uac->ctrl_setup;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
            request->bRequest = USB_REQ_SET_INTERFACE;
            request->wValue = 0;
            request->wIndex = uac->streams[uac->cur_stream].itf_num;
            request->wLength = 0;
            usbh_ctrl_xfer(uac->dev, uac, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        }
        else
        {
            uac->enum_stage = ENUM_STAGE_END;
            USB_LOGI("Handle %p: Enumeration completed", uac);
        }
        break;
    }
    }
}

static void uac_enum_task(uach_handle_t *uac)
{
    switch (uac->enum_stage)
    {
    case ENUM_STAGE_IDLE:
    {
        if (usbh_is_ctrl_xfering(uac->dev)) return;
        uac->cur_stream = 0;
        uac->enum_stage = ENUM_STAGE_CLOSE_INTERFACE;
        usb_setup_t *request = &uac->ctrl_setup;
        request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
        request->bRequest = USB_REQ_SET_INTERFACE;
        request->wValue = 0;
        request->wIndex = uac->streams[uac->cur_stream].itf_num;
        request->wLength = 0;
        usbh_ctrl_xfer(uac->dev, uac, request, NULL, CTRL_XFER_TIMEOUT, enum_ctrl_xfer_cb);
        break;
    }

    case ENUM_STAGE_ENUM_FAILED:
        uac->enum_stage = ENUM_STAGE_END;
        uac_drv_unbind_cb(uac->dev, uac->ctrl_itf->bInterfaceNumber);
        break;
    }
}

static const uach_stream_alt_setting_t *stream_alt_find(const uach_stream_handle_t *stream, uint8_t alt_setting)
{
    for (size_t i = 0; i < stream->alt_count; i++)
    {
        if (stream->alt_settings[i].itf->bAlternateSetting == alt_setting) return &stream->alt_settings[i];
    }
    return NULL;
}

static void stream_endp_close(uach_stream_handle_t *stream)
{
    if (stream->cur_alt)
    {
        usbh_endp_close(stream->uac->dev, stream->cur_alt->ep->bEndpointAddress);
        stream->cur_alt = NULL;
    }
    stream->cur_sample_rate = 0;
}

static bool stream_endp_open(uach_stream_handle_t *stream, const uach_stream_alt_setting_t *alt)
{
    if (!usbh_endp_open(stream->uac->dev, stream, (const usb_desc_endpoint_t *)alt->ep,
                        stream->dir == UACH_STREAM_DIR_CAPTURE ? uac_drv_stream_read_cb : uac_drv_stream_write_cb))
    {
        return false;
    }
    stream->cur_alt = alt;

    /* The sample rate is fixed if the format only advertises a single discrete rate */
    if (alt->format->bSamFreqType == 1)
    {
        memcpy(&stream->cur_sample_rate, &alt->format->tSamFreq[0], 3);
    }
    return true;
}

static void run_ctrl_xfer_cb(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t len)
{
    uach_stream_handle_t *stream = (uach_stream_handle_t *)handle;

    switch (stream->req_stage)
    {
    case REQ_STAGE_SET_ALT_BUSY:
    {
        if (!rst)
        {
            USB_LOGW("Handle %p: Failed to set stream interface %d alternate setting %d", stream->uac, stream->itf_num,
                     stream->req_alt);
            stream->req_rollback = false;
            stream->req_stage = REQ_STAGE_IDLE;
            if (stream->ctrl_cb)
            {
                stream->ctrl_cb(stream, rst);
            }
            break;
        }

        /* The data endpoint of the previous alternate setting is no longer valid */
        stream_endp_close(stream);
        if (stream->req_alt != 0 && !stream_endp_open(stream, stream_alt_find(stream, stream->req_alt)))
        {
            USB_LOGE("Handle %p: Failed to open stream interface %d endpoint, fall back to alternate setting 0",
                     stream->uac, stream->itf_num);
            if (!stream->req_rollback)
            {
                stream->req_rollback = true;
                stream->req_alt = 0;
                stream->req_stage = REQ_STAGE_SET_ALT;
                break;
            }
            rst = false;
        }
        else if (stream->req_alt != 0)
        {
            USB_LOGI("Handle %p: Stream interface %d alternate setting %d selected", stream->uac, stream->itf_num,
                     stream->req_alt);
        }
        else
        {
            USB_LOGI("Handle %p: Stream interface %d closed", stream->uac, stream->itf_num);
        }

        if (stream->req_rollback)
        {
            stream->req_rollback = false;
            rst = false;
        }
        stream->req_stage = REQ_STAGE_IDLE;
        if (stream->ctrl_cb)
        {
            stream->ctrl_cb(stream, rst);
        }
        break;
    }

    case REQ_STAGE_SET_RATE_BUSY:
    {
        if (rst)
        {
            stream->cur_sample_rate = stream->req_rate;
            USB_LOGI("Handle %p: Stream interface %d sample rate set to %u", stream->uac, stream->itf_num,
                     (unsigned int)stream->req_rate);
        }
        else
        {
            USB_LOGW("Handle %p: Failed to set stream interface %d sample rate %u", stream->uac, stream->itf_num,
                     (unsigned int)stream->req_rate);
        }
        stream->req_stage = REQ_STAGE_IDLE;
        if (stream->ctrl_cb)
        {
            stream->ctrl_cb(stream, rst);
        }
        break;
    }
    }
}

static void uac_run_task(uach_handle_t *uac)
{
    for (size_t i = 0; i < uac->stream_count; i++)
    {
        if (usbh_is_ctrl_xfering(uac->dev)) return;
        usb_setup_t *request = &uac->ctrl_setup;
        uach_stream_handle_t *stream = &uac->streams[i];

        switch (stream->req_stage)
        {
        case REQ_STAGE_SET_ALT:
            stream->req_stage = REQ_STAGE_SET_ALT_BUSY;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_STANDARD, USB_REQ_RCPT_INTERFACE);
            request->bRequest = USB_REQ_SET_INTERFACE;
            request->wValue = stream->req_alt;
            request->wIndex = stream->itf_num;
            request->wLength = 0;
            usbh_ctrl_xfer(uac->dev, stream, request, NULL, CTRL_XFER_TIMEOUT, run_ctrl_xfer_cb);
            break;

        case REQ_STAGE_SET_RATE:
            stream->req_stage = REQ_STAGE_SET_RATE_BUSY;
            request->bmRequestType = USB_SET_REQ(USB_DIR_OUT, USB_REQ_TYPE_CLASS, USB_REQ_RCPT_ENDPOINT);
            request->bRequest = UAC_REQ_SET_CUR;
            request->wValue = UAC_ENDP_SELECT_SAMPLING_FREQ << 8;
            request->wIndex = stream->cur_alt->ep->bEndpointAddress;
            request->wLength = 3;
            memcpy(uac->ctrl_buf, &stream->req_rate, 3);
            usbh_ctrl_xfer(uac->dev, stream, request, uac->ctrl_buf, CTRL_XFER_TIMEOUT, run_ctrl_xfer_cb);
            break;
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

size_t uach_get_stream_count(uach_handle_t *uac)
{
    if (!uac || uac->enum_stage < ENUM_STAGE_END) return 0;
    return uac->stream_count;
}

uach_stream_handle_t *uach_get_stream_handle(uach_handle_t *uac, size_t index)
{
    if (!uac || uac->enum_stage < ENUM_STAGE_END || index >= uac->stream_count) return NULL;
    return &uac->streams[index];
}

bool uach_stream_get_info(uach_stream_handle_t *stream, uach_stream_info_t *info)
{
    if (!stream || !info) return false;

    memset(info, 0, sizeof(uach_stream_info_t));
    info->itf_num = stream->itf_num;
    info->dir = stream->dir;
    info->terminal_link = stream->terminal_link;
    info->alt_count = stream->alt_count;
    info->cur_alt = stream->cur_alt ? stream->cur_alt->itf->bAlternateSetting : 0;
    info->channel_config = stream->channel_config;
    info->cur_sample_freq = stream->cur_sample_rate;
    return true;
}

bool uach_stream_get_alt_info(uach_stream_handle_t *stream, size_t index, uach_stream_alt_info_t *info)
{
    if (!stream || !info || index >= stream->alt_count) return false;

    const uach_stream_alt_setting_t *alt = &stream->alt_settings[index];
    const uac_desc_as_format_t *format = alt->format;
    memset(info, 0, sizeof(uach_stream_alt_info_t));
    info->alt_setting = alt->itf->bAlternateSetting;
    info->delay = alt->general->bDelay;
    info->format_tag = alt->general->wFormatTag;
    info->format_type = format->bFormatType;
    info->channels = format->bNrChannels;
    info->sub_frame_size = format->bSubframeSize;
    info->bit_resolution = format->bBitResolution;
    info->sample_freq_type = format->bSamFreqType;
    info->endp_addr = alt->ep->bEndpointAddress;
    info->sync_endp_addr = alt->sync_ep ? alt->sync_ep->bEndpointAddress : 0;
    info->sync_type = alt->ep->bmAttributes & UAC_ENDP_ATTR_SYNC_TYPE_MASK;
    info->interval = alt->ep->bInterval;
    info->max_packet_size = USB_ENDP_GET_MPS(alt->ep->wMaxPacketSize);
    info->lock_delay_units = alt->as_ep->bLockDelayUnits;
    info->lock_delay = alt->as_ep->wLockDelay;
    info->freq_ctrl = alt->as_ep->bmAttributes & UAC_AS_ENDP_ATTR_SAMPLING_FREQ ? true : false;
    info->pitch_ctrl = alt->as_ep->bmAttributes & UAC_AS_ENDP_ATTR_PITCH ? true : false;
    info->max_packets_only = alt->as_ep->bmAttributes & UAC_AS_ENDP_ATTR_MAX_PACKETS_ONLY ? true : false;

    if (format->bSamFreqType == 0)
    {
        memcpy(&info->min_sample_freq, &format->tSamFreq[0], 3);
        memcpy(&info->max_sample_freq, &format->tSamFreq[3], 3);
    }
    else
    {
        info->sample_freq_count = USB_MIN(format->bSamFreqType, USB_ARRAY_SIZE(info->sample_freq));
        for (size_t i = 0; i < info->sample_freq_count; i++)
        {
            memcpy(&info->sample_freq[i], &format->tSamFreq[i * 3], 3);
        }
    }
    return true;
}

bool uach_stream_set_alt_setting(uach_stream_handle_t *stream, uint8_t alt_setting, uach_stream_ctrl_cb cb)
{
    if (!stream || stream->req_stage != REQ_STAGE_IDLE) return false;
    if (alt_setting != 0 && !stream_alt_find(stream, alt_setting)) return false;

    stream->ctrl_cb = cb;
    stream->req_alt = alt_setting;
    stream->req_rollback = false;
    stream->req_stage = REQ_STAGE_SET_ALT;
    return true;
}

bool uach_stream_set_sample_rate(uach_stream_handle_t *stream, uint32_t sample_rate, uach_stream_ctrl_cb cb)
{
    if (!stream || stream->req_stage != REQ_STAGE_IDLE || !stream->cur_alt) return false;
    if (!(stream->cur_alt->as_ep->bmAttributes & UAC_AS_ENDP_ATTR_SAMPLING_FREQ)) return false;

    /* The sample rate shall be one of the rates advertised by the format type descriptor */
    const uac_desc_as_format_t *format = stream->cur_alt->format;
    if (format->bSamFreqType == 0)
    {
        uint32_t min_sample_freq;
        uint32_t max_sample_freq;
        memcpy(&min_sample_freq, &format->tSamFreq[0], 3);
        memcpy(&max_sample_freq, &format->tSamFreq[3], 3);
        if (sample_rate < min_sample_freq || sample_rate > max_sample_freq) return false;
    }
    else
    {
        size_t i = 0;
        for (; i < format->bSamFreqType; i++)
        {
            uint32_t freq;
            memcpy(&freq, &format->tSamFreq[i * 3], 3);
            if (freq == sample_rate) break;
        }
        if (i == format->bSamFreqType) return false;
    }

    stream->ctrl_cb = cb;
    stream->req_rate = sample_rate;
    stream->req_stage = REQ_STAGE_SET_RATE;
    return true;
}

bool uach_stream_capture(uach_stream_handle_t *stream, void *buf, size_t size, uach_stream_xfer_cb cb)
{
    if (!stream || !buf || !size || !stream->cur_alt || stream->req_stage != REQ_STAGE_IDLE) return false;
    if (stream->dir != UACH_STREAM_DIR_CAPTURE) return false;

    uint8_t endp = stream->cur_alt->ep->bEndpointAddress;
    if (size < USB_ENDP_GET_MPS(stream->cur_alt->ep->wMaxPacketSize)) return false;
    if (usbh_is_endp_xfering(stream->uac->dev, endp)) return false;

    stream->xfer_cb = cb;
    return usbh_endp_read(stream->uac->dev, endp, buf, size, 0);
}

bool uach_stream_playback(uach_stream_handle_t *stream, const void *buf, size_t size, uach_stream_xfer_cb cb)
{
    if (!stream || !buf || !size || !stream->cur_alt || stream->req_stage != REQ_STAGE_IDLE) return false;
    if (stream->dir != UACH_STREAM_DIR_PLAYBACK) return false;

    uint8_t endp = stream->cur_alt->ep->bEndpointAddress;
    if (size > USB_ENDP_GET_MPS(stream->cur_alt->ep->wMaxPacketSize)) return false;
    if (usbh_is_endp_xfering(stream->uac->dev, endp)) return false;

    stream->xfer_cb = cb;
    return usbh_endp_write(stream->uac->dev, endp, buf, size, 0);
}

static bool element_parse(const uac_desc_header_t *header, const uint8_t **elements)
{
    const uint8_t *curr = (const uint8_t *)header + header->bLength;
    const uint8_t *end = (const uint8_t *)header + header->wTotalLength;

    while (curr < end)
    {
        if (curr[0] < 4) return false;
        if (curr[1] != USB_DESC_CS_INTERFACE) return false;
        if (curr[2] != UAC_AC_DESC_INPUT_TERMINAL && curr[2] != UAC_AC_DESC_OUTPUT_TERMINAL &&
            curr[2] != UAC_AC_DESC_MIXER_UNIT && curr[2] != UAC_AC_DESC_SELECTOR_UNIT &&
            curr[2] != UAC_AC_DESC_FEATURE_UNIT && curr[2] != UAC_AC_DESC_PROCESSING_UNIT &&
            curr[2] != UAC_AC_DESC_EXTENSION_UNIT)
            return false;
        if (elements[curr[3]]) return false;
        elements[curr[3]] = (const uint8_t *)curr;
        curr += curr[0];
    }
    return curr == end;
}

static bool recursive_check(const uint8_t **elems, const uint8_t *curr, uint8_t *ch, uint16_t *config, uint8_t depth)
{
    if (!curr || depth > ELEMENT_MAX_DEPTH) return false;

    switch (curr[2])
    {
    case UAC_AC_DESC_INPUT_TERMINAL:
    {
        const uac_desc_input_terminal_t *input = (const uac_desc_input_terminal_t *)curr;
        if (input->bLength != sizeof(uac_desc_input_terminal_t)) return false;
        *ch = input->bNrChannels;
        *config = input->wChannelConfig;
        return true;
    }
    case UAC_AC_DESC_OUTPUT_TERMINAL:
    {
        const uac_desc_output_terminal_t *output = (const uac_desc_output_terminal_t *)curr;
        if (output->bLength != sizeof(uac_desc_output_terminal_t)) return false;
        return recursive_check(elems, elems[output->bSourceID], ch, config, depth + 1);
    }
    case UAC_AC_DESC_MIXER_UNIT:
    {
        const uac_desc_mixer_unit_t *mixer = (const uac_desc_mixer_unit_t *)curr;
        if (!mixer->bNrInPins) return false;
        for (size_t i = 0; i < mixer->bNrInPins; i++)
        {
            if (!recursive_check(elems, elems[curr[5 + i]], ch, config, depth + 1)) return false;
        }
        const uint8_t *mixer_bytes = (const uint8_t *)mixer;
        *ch = mixer_bytes[5 + mixer->bNrInPins];
        *config = mixer_bytes[6 + mixer->bNrInPins] | (mixer_bytes[7 + mixer->bNrInPins] << 8);
        return true;
    }
    case UAC_AC_DESC_SELECTOR_UNIT:
    {
        const uac_desc_select_unit_t *select = (const uac_desc_select_unit_t *)curr;
        if (!select->bNrInPins) return false;
        if (select->bLength != sizeof(uac_desc_select_unit_t) + select->bNrInPins) return false;
        uint8_t first_channels;
        uint16_t first_channel_config;
        for (size_t i = 0; i < select->bNrInPins; i++)
        {
            uint8_t channels = 0;
            uint16_t channel_config = 0;
            if (!recursive_check(elems, elems[curr[5 + i]], &channels, &channel_config, depth + 1)) return false;
            if (i == 0)
            {
                first_channels = channels;
                first_channel_config = channel_config;
            }
            else if (channels != first_channels || channel_config != first_channel_config)
            {
                return false;
            }
        }
        *ch = first_channels;
        *config = first_channel_config;
        return true;
    }
    case UAC_AC_DESC_FEATURE_UNIT:
    {
        const uac_desc_feature_unit_t *feature = (const uac_desc_feature_unit_t *)curr;
        if (!recursive_check(elems, elems[feature->bSourceID], ch, config, depth + 1)) return false;
        if (feature->bLength != sizeof(uac_desc_feature_unit_t) + ((*ch) + 1) * feature->bControlSize) return false;
        return true;
    }
    case UAC_AC_DESC_PROCESSING_UNIT:
    {
        const uac_desc_processing_unit_t *processing = (const uac_desc_processing_unit_t *)curr;
        if (!processing->bNrInPins) return false;
        for (size_t i = 0; i < processing->bNrInPins; i++)
        {
            if (!recursive_check(elems, elems[curr[7 + i]], ch, config, depth + 1)) return false;
        }
        return true;
    }
    case UAC_AC_DESC_EXTENSION_UNIT:
    {
        const uac_desc_extension_unit_t *extension = (const uac_desc_extension_unit_t *)curr;
        if (!extension->bNrInPins) return false;
        for (size_t i = 0; i < extension->bNrInPins; i++)
        {
            if (!recursive_check(elems, elems[curr[7 + i]], ch, config, depth + 1)) return false;
        }
        return true;
    }
    }
    return false;
}

static bool topology_check(const uint8_t **elems)
{
    for (size_t i = 1; i < ELEMENT_MAX_COUNT; i++)
    {
        if (elems[i] && ((const uint8_t *)elems[i])[2] == UAC_AC_DESC_OUTPUT_TERMINAL)
        {
            uint8_t channels = 0;
            uint16_t channel_config = 0;
            if (!recursive_check(elems, elems[i], &channels, &channel_config, 0))
            {
                return false;
            }
        }
    }
    return true;
}

static bool stream_alt_commit(uach_stream_handle_t *stream, const uach_stream_alt_setting_t *alt, const uint8_t **elems)
{
    if (!alt->general || !alt->format || !alt->ep || !alt->as_ep) return false;
    if (stream_alt_find(stream, alt->itf->bAlternateSetting)) return false;

    /* Only Type I and Type III formats are supported, skip the alternate settings of other formats */
    const uac_desc_as_format_t *format = alt->format;
    if (format->bFormatType == UAC_FORMAT_TYPE_II) return true;
    if (format->bFormatType != UAC_FORMAT_TYPE_I && format->bFormatType != UAC_FORMAT_TYPE_III) return false;

    uint8_t freq_count = format->bSamFreqType ? format->bSamFreqType : 2;
    if (format->bLength != sizeof(uac_desc_as_format_t) + freq_count * 3) return false;
    if (format->bNrChannels == 0 || format->bSubframeSize < 1 || format->bSubframeSize > 4) return false;
    if (format->bBitResolution == 0 || format->bBitResolution > format->bSubframeSize * 8) return false;
    if (format->bSamFreqType == 0)
    {
        uint32_t min_sample_freq;
        uint32_t max_sample_freq;
        memcpy(&min_sample_freq, &format->tSamFreq[0], 3);
        memcpy(&max_sample_freq, &format->tSamFreq[3], 3);
        if (min_sample_freq > max_sample_freq) return false;
    }

    /* The data endpoint is isochronous, and the optional synch endpoint is the one it refers to */
    const uac_desc_audio_endpoint_t *ep = alt->ep;
    uint8_t sync_addr = alt->sync_ep ? alt->sync_ep->bEndpointAddress : 0;
    if (ep->bLength != 9) return false;
    if (USB_ENDP_GET_TYPE(ep->bmAttributes) != USB_ENDP_TYPE_ISOC) return false;
    if (USB_ENDP_GET_USAGE_TYPE(ep->bmAttributes) == USB_ENDP_USAGE_TYPE_FEEDBACK) return false;
    if (ep->bSynchAddress != sync_addr) return false;
    if (alt->sync_ep && USB_ENDP_GET_TYPE(alt->sync_ep->bmAttributes) != USB_ENDP_TYPE_ISOC) return false;

    /* The terminal link shall be a USB streaming terminal, which determines the stream direction */
    const uint8_t *term = elems[alt->general->bTerminalLink];
    if (!term || (uint16_t)(term[4] | (term[5] << 8)) != UAC_TERMINAL_USB_TYPE_STREAMING) return false;
    if (term[2] != UAC_AC_DESC_INPUT_TERMINAL && term[2] != UAC_AC_DESC_OUTPUT_TERMINAL) return false;
    uach_stream_dir_t dir = term[2] == UAC_AC_DESC_INPUT_TERMINAL ? UACH_STREAM_DIR_PLAYBACK : UACH_STREAM_DIR_CAPTURE;
    if ((USB_ENDP_DIR(ep->bEndpointAddress) != 0) != (dir == UACH_STREAM_DIR_CAPTURE)) return false;

    uint8_t channels = 0;
    uint16_t channel_config = 0;
    if (!recursive_check(elems, term, &channels, &channel_config, 0)) return false;
    if (stream->terminal_link && stream->terminal_link != alt->general->bTerminalLink) return false;
    if (channels != format->bNrChannels)
    {
        USB_LOGW("Stream interface %d alternate setting %d: channels %d mismatch terminal %d", stream->itf_num,
                 alt->itf->bAlternateSetting, format->bNrChannels, channels);
    }

    stream->dir = dir;
    stream->terminal_link = alt->general->bTerminalLink;
    stream->channel_config = channel_config;
    if (stream->alt_count >= USB_ARRAY_SIZE(stream->alt_settings))
    {
        USB_LOGW("Stream interface %d: Alternate setting %d ignored, too many alternate settings", stream->itf_num,
                 alt->itf->bAlternateSetting);
        return true;
    }
    stream->alt_settings[stream->alt_count] = *alt;
    stream->alt_count++;
    return true;
}

static bool stream_parse(usbh_device_t *dev, uach_stream_handle_t *stream, const uint8_t **elems, bool *usable)
{
    bool has_alt0 = false;
    bool in_alt = false;
    uint8_t endp_index = 0;
    uint8_t endp_count = 0;
    uach_stream_alt_setting_t alt = {0};

    const uint8_t *curr = (const uint8_t *)dev->config_desc;
    const uint8_t *end = curr + ((const usb_desc_config_t *)dev->config_desc)->wTotalLength;
    while (curr < end)
    {
        uint8_t len = curr[0];
        uint8_t type = curr[1];

        switch (type)
        {
        case USB_DESC_INTERFACE:
        {
            if (in_alt)
            {
                if (endp_count == 2 && alt.sync_ep == NULL) return false;
                if (endp_index != endp_count || !stream_alt_commit(stream, &alt, elems)) return false;
                in_alt = false;
            }

            const usb_desc_interface_t *itf = (const usb_desc_interface_t *)curr;
            if (itf->bInterfaceNumber != stream->itf_num)
            {
                break;
            }
            if (itf->bInterfaceClass != USB_CLASS_AUDIO || itf->bInterfaceSubClass != UAC_SUBCLASS_AUDIO_STREAMING ||
                itf->bInterfaceProtocol != UAC_PROTOCOL_UNDEFINED)
                return false;

            if (itf->bAlternateSetting == 0)
            {
                /* The zero bandwidth alternate setting has no endpoint */
                if (itf->bNumEndpoints != 0) return false;
                has_alt0 = true;
            }
            else
            {
                if (!itf->bNumEndpoints || itf->bNumEndpoints > 2) return false;
                memset(&alt, 0, sizeof(alt));
                alt.itf = itf;
                endp_index = 0;
                endp_count = itf->bNumEndpoints;
                in_alt = true;
            }
            break;
        }
        case USB_DESC_CS_INTERFACE:
        {
            if (!in_alt) break;
            uint8_t sub_type = curr[2];
            if (sub_type == UAC_AS_DESC_GENERAL)
            {
                if (alt.general || len != sizeof(uac_desc_as_itf_t)) return false;
                alt.general = (const uac_desc_as_itf_t *)curr;
            }
            else if (sub_type == UAC_AS_DESC_FORMAT_TYPE)
            {
                if (!alt.general || alt.format || len < sizeof(uac_desc_as_format_t)) return false;
                alt.format = (const uac_desc_as_format_t *)curr;
            }
            break;
        }
        case USB_DESC_ENDPOINT:
        {
            if (!in_alt) break;
            if (endp_index == 0)
            {
                alt.ep = (const uac_desc_audio_endpoint_t *)curr;
            }
            else
            {
                alt.sync_ep = (const uac_desc_audio_endpoint_t *)curr;
            }
            endp_index++;
            break;
        }
        case USB_DESC_CS_ENDPOINT:
        {
            if (!in_alt) break;
            uint8_t sub_type = curr[2];

            /* The class-specific descriptor belongs to the data endpoint */
            if (endp_index != 1 || alt.as_ep || len != sizeof(uac_desc_audio_as_endpoint_t)) return false;
            if (sub_type != UAC_ENDP_SUBTYPE_GENERAL) return false;
            alt.as_ep = (const uac_desc_audio_as_endpoint_t *)curr;
            break;
        }
        }
        curr += len;
    }

    if (in_alt)
    {
        if (endp_count == 2 && alt.sync_ep == NULL) return false;
        if (endp_index != endp_count || !stream_alt_commit(stream, &alt, elems)) return false;
    }

    if (!has_alt0) return false;
    *usable = stream->alt_count > 0;
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

static bool uac_drv_bind_cb(usbh_device_t *dev, const void *desc, uint16_t len)
{
    if (!dev || dev->speed == USB_SPEED_LOW || !desc || len == 0) return false;

    const usb_desc_interface_t *ctrl_itf = NULL;
    const uac_desc_header_t *header_desc = NULL;

    const uint8_t *curr = (const uint8_t *)desc;
    const uint8_t *end = curr + len;

    while (curr < end)
    {
        uint8_t len = curr[0];
        uint8_t type = curr[1];

        if (type == USB_DESC_INTERFACE)
        {
            ctrl_itf = (const usb_desc_interface_t *)curr;
            if (ctrl_itf->bAlternateSetting != 0 || ctrl_itf->bNumEndpoints > 1) return false;
        }
        else if (type == USB_DESC_CS_INTERFACE)
        {
            header_desc = (const uac_desc_header_t *)curr;
            if (curr + sizeof(uac_desc_header_t) > end || header_desc->bDescriptorSubType != UAC_AC_DESC_HEADER)
                return false;
            if (header_desc->bcdADC != SUPPORTED_PROTOCOL_VERSION || header_desc->bInCollection == 0) return false;
            if (header_desc->bLength != sizeof(uac_desc_header_t) + header_desc->bInCollection) return false;
            if (header_desc->wTotalLength < header_desc->bLength || curr + header_desc->wTotalLength > end)
                return false;
            break;
        }
        curr += len;
    }

    if (!ctrl_itf || !header_desc) return false;

    const uint8_t *elements[ELEMENT_MAX_COUNT] = {0};
    if (!element_parse(header_desc, elements) || !topology_check(elements))
    {
        USB_LOGE("Handle %p: Invalid AudioControl topology", dev);
        return false;
    }

    size_t stream_count = 0;
    uach_stream_handle_t streams[UACH_MAX_STREAM_ITF_COUNT] = {0};
    for (size_t i = 0; i < header_desc->bInCollection; i++)
    {
        uint8_t itf_num = header_desc->baInterfaceNr[i];
        if (stream_count >= USB_ARRAY_SIZE(streams))
        {
            USB_LOGW("Handle %p: Stream interface %d ignored, too many stream interfaces", dev, itf_num);
            continue;
        }

        uach_stream_handle_t *stream = &streams[stream_count];
        memset(stream, 0, sizeof(uach_stream_handle_t));
        stream->itf_num = itf_num;
        bool duplicate = false;
        for (size_t j = 0; j < stream_count; j++)
        {
            if (streams[j].itf_num == itf_num)
            {
                USB_LOGE("Handle %p: Duplicate stream interface %d", dev, itf_num);
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        bool usable = false;
        if (!stream_parse(dev, stream, elements, &usable))
        {
            USB_LOGE("Handle %p: Failed to parse stream interface %d", dev, itf_num);
            return false;
        }

        if (usable)
        {
            stream_count++;
        }
        else
        {
            USB_LOGW("Handle %p: Stream interface %d has no supported format", dev, itf_num);
        }
    }
    if (stream_count == 0)
    {
        USB_LOGE("Handle %p: No usable stream interface", dev);
        return false;
    }

    /* Allocate and initialize a new uac handle */
    uach_handle_t *uac = handle_alloc();
    if (!uac)
    {
        USB_LOGE("Handle %p: Failed to allocate uac handle", dev);
        return false;
    }

    uac->dev = dev;
    uac->ctrl_itf = ctrl_itf;
    uac->stream_count = stream_count;
    for (size_t i = 0; i < stream_count; i++)
    {
        streams[i].uac = uac;
        memcpy(&uac->streams[i], &streams[i], sizeof(uach_stream_handle_t));
    }
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
                stream_endp_close(&uac->streams[i]);
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
    uach_stream_handle_t *stream = (uach_stream_handle_t *)handle;
    if (!rst || !stream || !stream->cur_alt) return;
    if (stream->xfer_cb)
    {
        stream->xfer_cb(stream, (void *)buf, len);
    }
}

static void uac_drv_stream_write_cb(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len)
{
    uach_stream_handle_t *stream = (uach_stream_handle_t *)handle;
    if (!rst || !stream || !stream->cur_alt) return;
    if (stream->xfer_cb)
    {
        stream->xfer_cb(stream, (void *)buf, len);
    }
}

#endif // USB_CLASS_UACH_DRIVER_EN
