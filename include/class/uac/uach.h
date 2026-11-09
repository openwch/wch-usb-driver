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
typedef struct uach_stream_handle uach_stream_handle_t;

/* @function pointer */

/**
 * @brief Stream control completion callback (set alternate setting / set sample rate)
 * 
 * @param stream Stream handle
 * @param rst Operation result
 */
typedef void (*uach_stream_ctrl_cb)(uach_stream_handle_t *stream, bool rst);

/**
 * @brief Stream data transfer completion callback (capture / playback)
 * 
 * @param stream Stream handle
 * @param buf Buffer used by the transfer
 * @param len Actual transfer length
 */
typedef void (*uach_stream_xfer_cb)(uach_stream_handle_t *stream, void *buf, size_t len);

/* @struct */
typedef struct
{
    uint8_t itf_num;          // AudioStreaming interface number
    uint8_t dir;              // Stream direction, see uach_stream_dir_t
    uint8_t terminal_link;    // ID of the USB streaming terminal linked to the stream
    uint8_t alt_count;        // Number of usable non-zero alternate settings
    uint8_t cur_alt;          // Current alternate setting, 0 means idle (zero bandwidth)
    uint16_t channel_config;  // Spatial location bitmap of the linked terminal
    uint32_t cur_sample_freq; // Current sample rate in Hz, 0 means unknown
} uach_stream_info_t;

typedef struct
{
    uint8_t alt_setting;                        // bAlternateSetting
    uint8_t delay;                              // bDelay of AS_GENERAL, in frames
    uint8_t format_type;                        // bFormatType, see uac_format_type_t
    uint8_t channels;                           // bNrChannels
    uint8_t sub_frame_size;                     // bSubframeSize, bytes per audio subframe
    uint8_t bit_resolution;                     // bBitResolution
    uint8_t sample_freq_type;                   // bSamFreqType: 0 continuous range, otherwise number of discrete rates
    uint8_t sample_freq_count;                  // Valid entries in sample_freq, limited by UACH_MAX_SAMPLE_FREQ
    uint8_t endp_addr;                          // Isochronous data endpoint address
    uint8_t sync_endp_addr;                     // Isochronous synch endpoint address, 0 if none
    uint8_t sync_type;                          // Endpoint synchronization type, UAC_ENDP_ATTR_SYNC_TYPE_*
    uint8_t interval;                           // bInterval of the data endpoint
    uint8_t lock_delay_units;                   // bLockDelayUnits
    uint16_t lock_delay;                        // wLockDelay
    uint16_t max_packet_size;                   // Max packet size of the data endpoint
    uint16_t format_tag;                        // wFormatTag, see uac_format_tag_t
    bool freq_ctrl;                             // Endpoint supports the sampling frequency control
    bool pitch_ctrl;                            // Endpoint supports the pitch control
    bool max_packets_only;                      // Endpoint requires packets of wMaxPacketSize only
    uint32_t min_sample_freq;                   // Minimum sample rate, only valid if sample_freq_type is 0
    uint32_t max_sample_freq;                   // Maximum sample rate, only valid if sample_freq_type is 0
    uint32_t sample_freq[UACH_MAX_SAMPLE_FREQ]; // Discrete sample rates, only valid if sample_freq_type is not 0
} uach_stream_alt_info_t;

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
 * @brief Get the number of AudioStreaming interfaces of the USB UAC host driver instance
 * 
 * @param uac UAC instance handle
 * 
 * @return The number of stream interfaces
 */
size_t uach_get_stream_count(uach_handle_t *uac);

/**
 * @brief Get the stream interface handle by index
 * 
 * @param uac UAC instance handle
 * @param index The index of the stream interface
 * 
 * @return Stream handle, or NULL if the index is invalid
 */
uach_stream_handle_t *uach_get_stream_handle(uach_handle_t *uac, size_t index);

/**
 * @brief Get the stream interface information
 * 
 * @param stream Stream handle
 * @param info Pointer to store the stream interface information
 * 
 * @return Operation result
 */
bool uach_stream_get_info(uach_stream_handle_t *stream, uach_stream_info_t *info);

/**
 * @brief Get the information of a non-zero alternate setting of the stream interface
 * 
 * @param stream Stream handle
 * @param index The index of the alternate setting, in range [0, uach_stream_info_t.alt_count)
 * @param info Pointer to store the alternate setting information
 * 
 * @return Operation result
 */
bool uach_stream_get_alt_info(uach_stream_handle_t *stream, size_t index, uach_stream_alt_info_t *info);

/**
 * @brief Select the alternate setting of the stream interface
 * 
 * @param stream Stream handle
 * @param alt_setting The alternate setting to select, 0 or bAlternateSetting of a usable alternate setting
 * @param cb Callback invoked when the request is complete, can be NULL
 * 
 * @return true if the request is accepted, false if the parameter is invalid or another request is pending
 */
bool uach_stream_set_alt_setting(uach_stream_handle_t *stream, uint8_t alt_setting, uach_stream_ctrl_cb cb);

/**
 * @brief Set the sample rate of the active alternate setting through the endpoint sampling frequency control
 * 
 * @param stream Stream handle
 * @param sample_rate Sample rate in Hz
 * @param cb Callback invoked when the request is complete, can be NULL
 * 
 * @return true if the request is accepted, false if the parameter is invalid or another request is pending
 */
bool uach_stream_set_sample_rate(uach_stream_handle_t *stream, uint32_t sample_rate, uach_stream_ctrl_cb cb);

/**
 * @brief Capture one isochronous packet from the stream
 * 
 * @param stream Stream handle
 * @param buf Buffer to store the captured data, its size shall not be less than the max packet size
 * @param size Size of the buffer
 * @param cb Callback invoked when the packet is received, can be NULL
 * 
 * @return true if the transfer is submitted, false if the stream is not capturing or the parameter is invalid
 */
bool uach_stream_capture(uach_stream_handle_t *stream, void *buf, size_t size, uach_stream_xfer_cb cb);

/**
 * @brief Play one isochronous packet to the stream
 * 
 * @param stream Stream handle
 * @param buf Buffer containing the audio data, its size shall not exceed the max packet size
 * @param size Size of the data
 * @param cb Callback invoked when the packet is sent, can be NULL
 * 
 * @return true if the transfer is submitted, false if the stream is not playing or the parameter is invalid
 */
bool uach_stream_playback(uach_stream_handle_t *stream, const void *buf, size_t size, uach_stream_xfer_cb cb);

#ifdef __cplusplus
}
#endif

#endif // UACH_H
