/**
 * @file usbd_driver_private.h
 * @author Links (lhd@wch.cn)
 * @brief USB device core private header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USBD_DRIVER_CORE_PRIVATE_H
#define USBD_DRIVER_CORE_PRIVATE_H

/* @include */
#include "usb_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @enum */
typedef enum
{
    USBD_EVENT_XFER,
    USBD_EVENT_SOF,
    USBD_EVENT_SETUP,
    USBD_EVENT_RESET,
    USBD_EVENT_SUSPEND,
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
            usb_setup_t *setup;
        } setup;

        struct
        {
            void *buf;
            size_t len;
            usb_endp_t endp;
        } xfer;
    };
} usbd_event_ctx_t;

typedef struct
{
    uint16_t mps;
    size_t xfer_len;
    size_t xfer_ofs;
} usbd_endp_ctx_t;

typedef struct usbd_handle
{
    uint32_t base_addr;

    bool self_powered;
    bool remote_wakeup;
    uint8_t ep0_mps;
    uint8_t dev_addr;
    uint8_t link_speed;
    uint8_t config_num;

    usbd_endp_ctx_t endp_ctxs[2][USB_MAX_ENDP_NUM];

    __attribute__((aligned(4))) usb_setup_t setup;

    /* USB device operations */
    bool (*open)(usbd_handle_t *h, usb_speed_t speed, bool sof_en);
    bool (*close)(usbd_handle_t *h);

    bool (*resume)(usbd_handle_t *h);

    bool (*set_address)(usbd_handle_t *h, uint8_t address);
    bool (*test_mode_ctrl)(usbd_handle_t *h, usb_test_mode_t test_mode);

    bool (*endp_open)(usbd_handle_t *h, usb_endp_t endp, usb_endp_type_t type, uint16_t mps);
    bool (*endp_close)(usbd_handle_t *h, usb_endp_t endp);
    bool (*endp_stall)(usbd_handle_t *h, usb_endp_t endp, bool stall);
    bool (*endp_is_stalled)(usbd_handle_t *h, usb_endp_t endp);

    bool (*write)(usbd_handle_t *h, usb_endp_t endp, const void *data, size_t len);
    bool (*read)(usbd_handle_t *h, usb_endp_t endp, void *data, size_t len);
} usbd_handle_t;

/* @function declaration */
void usbd_event_handle(usbd_handle_t *h, usbd_event_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // USBD_DRIVER_CORE_PRIVATE_H
