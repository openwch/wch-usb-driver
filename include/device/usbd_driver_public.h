/**
 * @file usbd_driver_public.h
 * @author Links (lhd@wch.cn)
 * @brief USB device core public header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USBD_DRIVER_PUBLIC_H
#define USBD_DRIVER_PUBLIC_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @enum */
typedef enum
{
    USBD_EVENT_RESET,
    USBD_EVENT_SUSPEND,
    USBD_EVENT_SOF,
    USBD_EVENT_ENUM_COMPLETED,
    USBD_EVENT_COUNT,
} usbd_event_t;

/* @struct */
typedef struct
{
    usbd_event_t e;

    union
    {
        struct
        {
            uint16_t frame_num;
            uint16_t mframe_num;
        } sof;

        struct
        {
            uint8_t config_num;
            uint8_t link_speed;
        } enum_completed;
    };
} usbd_event_ctx_t;

/* @typedef */
typedef struct usbd_handle usbd_handle_t;

/* @function pointer */

/**
 * @brief USB device event callback function
 * 
 * @param h USB device driver handle
 * @param ctx Context of the USB device event
 */
typedef void (*usbd_event_cb)(usbd_handle_t *h, usbd_event_ctx_t *ctx);

/**
 * @brief USB device get descriptor callback function
 * @param h USB device driver handle
 * @param type Type of the descriptor to get
 * @param index Index of the descriptor
 * @param speed USB device speed
 * @param len Pointer to store the length of the descriptor
 * 
 * @return Pointer to the requested descriptor
 */
typedef const void *(*usbd_get_desc_cb)(usbd_handle_t *h, uint8_t type, uint8_t index, usb_speed_t speed, size_t *len);

/* @function declaration */

/**
 * @brief Open the USB device driver
 * 
 * @param h USB device driver handle
 * @param speed USB device speed
 * @param sof_en Enable or disable SOF (Start of Frame) events
 * @param get_desc_cb Callback function to get USB descriptors
 */
bool usbd_drv_open(usbd_handle_t *h, usb_speed_t speed, bool sof_en, usbd_get_desc_cb get_desc_cb);

/**
 * @brief Close the USB device driver
 * 
 * @param h USB device driver handle
 */
bool usbd_drv_close(usbd_handle_t *h);

/**
 * @brief Send a resume signal
 * 
 * @param h USB device driver handle
 */
bool usbd_drv_resume(usbd_handle_t *h);

/**
 * @brief Register an event callback for the USB device driver
 * 
 * @param h USB device driver handle
 * @param event USB device event to register the callback for
 * @param cb Callback function to handle the event
 */
bool usbd_register_event_callback(usbd_handle_t *h, usbd_event_t event, usbd_event_cb cb);

/**
 * @brief Unregister an event callback for the USB device driver
 * 
 * @param h USB device driver handle
 * @param event USB device event to unregister the callback for
 */
bool usbd_unregister_event_callback(usbd_handle_t *h, usbd_event_t event);

#ifdef __cplusplus
}
#endif

#endif // USBD_DRIVER_PUBLIC_H
