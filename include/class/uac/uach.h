/********************************** (C) COPYRIGHT *******************************
* File Name          : uach.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB UAC host class driver
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef UACH_H
#define UACH_H

/* @include */
#include "usb_define.h"
#include "class/uac/uac.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define UACH_MAX_SAMPLE_FREQ 8

/* @enum */
typedef enum
{
    UACH_STREAM_DIR_CAPTURE,
    UACH_STREAM_DIR_PLAYBACK,
} uach_stream_dir_t;

/* @typedef */
typedef struct uach_handle uach_handle_t;

/* @function pointer */
typedef void (*uach_stream_open_cb)(void *stream_handle, bool rst);
typedef void (*uach_stream_transfer_cb)(void *stream_handle, void *buf, size_t size);

/* @struct */
typedef struct
{
    uint8_t dir;
    uint8_t delay;
    uint8_t format_type;
    uint8_t channels;
    uint8_t sub_frame_size;
    uint8_t bit_resolution;
    uint8_t sample_freq_type;
    uint16_t format_tag;
    uint16_t channel_config;
    uint32_t min_sample_freq;
    uint32_t max_sample_freq;
    uint32_t sample_freq[UACH_MAX_SAMPLE_FREQ];
    void *stream_handle;
} uach_stream_info_t;

/* @function declarations */

/**
 * @brief Handle the USB UAC host driver tasks
 * 
 */
void uach_drv_task(void);

/**
 * @brief Open the USB UAC host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool uach_drv_open(usbh_handle_t *h);

/**
 * @brief Close the USB UAC host driver
 * 
 * @param h USB host handle
 * 
 * @return Operation result
 */
bool uach_drv_close(usbh_handle_t *h);

/**
 * @brief Get the number of USB UAC host driver instances
 * 
 * @return The number of USB UAC host driver instances
 */
size_t uach_get_count(void);

/**
 * @brief Get the USB UAC host driver handle by index
 * 
 * @param index The index of the USB UAC host driver instance
 * 
 * @return UAC instance handle, or NULL if the index is invalid
 */
uach_handle_t *uach_get_handle(size_t index);

/**
 * @brief Get the stream information of the USB UAC host driver
 * 
 * @param uac UAC instance handle
 * @param info Pointer to the array to store stream information
 * @param max_count Maximum number of stream information entries to retrieve
 * 
 * @return The number of stream information entries retrieved
 */
size_t uach_get_stream_info(uach_handle_t *uac, uach_stream_info_t *info, size_t max_count);

/**
 * @brief Open a USB UAC stream interface
 * 
 * @param handle Stream handle
 * @param sample_rate Sample rate for the stream
 * @param cb Callback function to be called when the stream is opened
 */
void uach_stream_open(void *handle, uint32_t sample_rate, uach_stream_open_cb cb);

/**
 * @brief Close a USB UAC stream interface
 * 
 * @param handle Stream handle to be closed
 */
void uach_stream_close(void *handle);

/**
 * @brief Check if a USB UAC stream interface is open
 * 
 * @param handle Stream handle to check
 * 
 * @return true if the stream is open, false otherwise
 */
bool uach_stream_is_open(void *handle);

/**
 * @brief Start a USB UAC capture stream
 * 
 * @param handle Stream handle
 * @param buf Buffer to store captured data
 * @param size Size of the buffer
 * @param cb Callback function to be called when the transfer is complete
 * 
 * @return Operation result
 */
bool uach_stream_capture(void *handle, void *buf, size_t size, uach_stream_transfer_cb cb);

/**
 * @brief Start a USB UAC playback stream
 * 
 * @param handle Stream handle
 * @param buf Buffer containing data to be played back
 * @param size Size of the buffer
 * @param cb Callback function to be called when the transfer is complete
 * 
 * @return Operation result
 */
bool uach_stream_playback(void *handle, void *buf, size_t size, uach_stream_transfer_cb cb);

#ifdef __cplusplus
}
#endif

#endif // UACH_H
