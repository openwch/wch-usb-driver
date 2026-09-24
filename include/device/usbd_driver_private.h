/**
 * @file usbd_driver_private.h
 * @author Links (lhd@wch.cn)
 * @brief USB device core private header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USBD_DRIVER_PRIVATE_H
#define USBD_DRIVER_PRIVATE_H

/* @include */
#include "usb_define.h"
#include "usbd_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @enum */
typedef enum
{
    USBD_PORT_EVENT_XFER,
    USBD_PORT_EVENT_SOF,
    USBD_PORT_EVENT_SETUP,
    USBD_PORT_EVENT_RESET,
    USBD_PORT_EVENT_SUSPEND,
} usbd_port_event_t;

/* @function pointer */
typedef bool (*usbd_ctrl_xfer_cb)(usbd_handle_t *h, const usb_setup_t *setup, void **buf, size_t *len);
typedef bool (*usbd_data_xfer_cb)(usbd_handle_t *h, usb_endp_t endp, void *buf, size_t len);

/* @struct */
typedef struct
{
    usbd_port_event_t e;

    union
    {
        struct
        {
            uint16_t frame_num;
            uint16_t mframe_num;
        } sof;

        struct
        {
            void *buf;
            size_t len;
            usb_endp_t endp;
        } xfer;
    };
} usbd_port_event_ctx_t;

typedef struct
{
    uint16_t mps;
    void *xfer_buf;
    size_t xfer_len;
    size_t xfer_ofs;
    usbd_data_xfer_cb cb;
} usbd_endp_ctx_t;

typedef struct
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    usbd_ctrl_xfer_cb cb[3];
} usbd_request_cb_t;

typedef struct
{
    usbd_ctrl_xfer_cb cb[3];
} usbd_interface_cb_t;

typedef struct usbd_handle
{
    /* Base address of the USB device controller */
    uint32_t base_addr;

    /* USB device information */
    bool self_powered;
    bool remote_wakeup;
    uint8_t ep0_mps;
    uint8_t link_speed;
    uint8_t config_num;

    /* USB Standard Request Temporary Buffer */
    uint32_t stand_req_buf;

    /* Endpoint transfer contexts */
    usbd_endp_ctx_t endp_ctxs[2][USB_MAX_ENDP_NUM];

    /* Setup packet buffer aligned to 4 bytes */
    __attribute__((aligned(4))) usb_setup_t setup;

    /* Current control transfer stage callbacks */
    usbd_ctrl_xfer_cb data_stage_cb;
    usbd_ctrl_xfer_cb status_stage_cb;

    /* Event callback functions */
    usbd_event_cb event_cbs[USBD_EVENT_COUNT];

    /* Control transfer callback functions */
    usbd_request_cb_t request_cbs[USBD_REQUEST_CB_COUNT];
    usbd_interface_cb_t interface_cbs[USBD_INTERFACE_CB_COUNT];

    /* USB device operations */
    bool (*open)(usbd_handle_t *h, usb_speed_t speed, bool sof_en);
    bool (*close)(usbd_handle_t *h);

    bool (*resume)(usbd_handle_t *h);

    bool (*set_address)(usbd_handle_t *h, uint8_t address);
    bool (*test_mode_ctrl)(usbd_handle_t *h, usb_test_select_t test_mode);

    bool (*endp_open)(usbd_handle_t *h, usb_endp_t endp, usb_endp_type_t type, uint16_t mps);
    bool (*endp_close)(usbd_handle_t *h, usb_endp_t endp);
    bool (*endp_stall)(usbd_handle_t *h, usb_endp_t endp, bool stall);
    bool (*endp_is_stalled)(usbd_handle_t *h, usb_endp_t endp);
    bool (*endp_transfer)(usbd_handle_t *h, usb_endp_t endp, void *buf, size_t len);
} usbd_handle_t;

/* @function declaration */
void usbd_event_handle(usbd_handle_t *h, usbd_port_event_ctx_t *ctx);
bool usbd_register_request_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest, usbd_ctrl_xfer_cb cb[3]);
bool usbd_register_interface_cb(usbd_handle_t *h, uint8_t interface_num, usbd_ctrl_xfer_cb cb[3]);
bool usbd_unregister_request_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest);
bool usbd_unregister_interface_cb(usbd_handle_t *h, uint8_t interface_num);

#ifdef __cplusplus
}
#endif

#endif // USBD_DRIVER_PRIVATE_H
