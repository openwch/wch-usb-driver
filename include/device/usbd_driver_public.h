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
        } enum_completed;
    };
} usbd_event_ctx_t;

/* @typedef */
typedef struct usbd_handle usbd_handle_t;

/* @function pointer */
typedef void (*usbd_event_cb)(usbd_handle_t *h, usbd_event_ctx_t *ctx);

/* @function declaration */
bool usbd_drv_open(usbd_handle_t *h, usb_speed_t speed, bool sof_en);
bool usbd_register_event_callback(usbd_handle_t *h, usbd_event_t event, usbd_event_cb cb);
bool usbd_unregister_event_callback(usbd_handle_t *h, usbd_event_t event);

#ifdef __cplusplus
}
#endif

#endif // USBD_DRIVER_PUBLIC_H
