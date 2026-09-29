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
    bool (*cb)(usbd_handle_t *h, usb_endp_t endp, void *buf, size_t len);
} usbd_endp_ctx_t;

typedef struct
{
    bool (*setup)(void *handle, const usb_setup_t *setup, void **buf, size_t *len);
    bool (*data)(void *handle, const usb_setup_t *setup, void *buf, size_t len);
    void (*status)(void *handle, const usb_setup_t *setup, void *buf, size_t len);
} usbd_ctrl_xfer_cbs_t;

typedef struct
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    usbd_ctrl_xfer_cbs_t cbs;
} usbd_request_cbs_t;

typedef struct
{
    void *itf_handle;
    usbd_ctrl_xfer_cbs_t cbs;
} usbd_interface_cbs_t;

typedef struct usbd_handle
{
    /* Base address of the USB device controller */
    uint32_t base_addr;

    /* USB device information */
    bool remote_wakeup;
    uint8_t ep0_mps;
    uint8_t link_speed;
    uint8_t config_num;

    /* USB device get descriptor callback */
    usbd_get_desc_cb get_desc_cb;

    /* USB Standard Request Temporary Buffer */
    uint32_t stand_req_buf;

    /* Endpoint transfer contexts */
    usbd_endp_ctx_t endp_ctxs[2][USB_MAX_ENDP_NUM];

    /* Endpoint 0 buffer aligned to 4 bytes */
    __attribute__((aligned(4))) uint8_t ep0_buf[USB_ENDP0_MAX_LEN];

    /* Event callback functions */
    usbd_event_cb event_cbs[USBD_EVENT_COUNT];

    /* Control transfer context */
    usb_setup_t setup;
    void *ctrl_handle;
    bool ctrl_xfer_zlp;
    void *ctrl_xfer_buf;
    size_t ctrl_xfer_len;
    usbd_ctrl_xfer_cbs_t *ctrl_cbs;

    /* Request and interface callbacks */
    usbd_request_cbs_t request_cbs[USBD_REQUEST_CB_COUNT];
    usbd_interface_cbs_t interface_cbs[USBD_INTERFACE_CB_COUNT];

    /* USB device operations */
    bool (*open)(usbd_handle_t *h, usb_speed_t speed, bool sof_en);
    bool (*close)(usbd_handle_t *h);

    bool (*resume)(usbd_handle_t *h);

    bool (*set_address)(usbd_handle_t *h, uint8_t address);
    bool (*test_mode_ctrl)(usbd_handle_t *h, usb_test_select_t test_mode);
    usb_speed_t (*get_link_speed)(usbd_handle_t *h);

    bool (*endp_open)(usbd_handle_t *h, usb_endp_t endp, usb_endp_type_t type, uint16_t mps);
    bool (*endp_close)(usbd_handle_t *h, usb_endp_t endp);
    bool (*endp_stall)(usbd_handle_t *h, usb_endp_t endp, bool stall);
    bool (*endp_is_stalled)(usbd_handle_t *h, usb_endp_t endp);
    bool (*endp_transfer)(usbd_handle_t *h, usb_endp_t endp, void *buf, size_t len);
} usbd_handle_t;

/* @function declaration */
void usbd_event_handle(usbd_handle_t *h, usbd_port_event_ctx_t *ctx);
bool usbd_register_request_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest, usbd_ctrl_xfer_cbs_t *cbs);
bool usbd_register_interface_cb(usbd_handle_t *h, void *itf_handle, uint8_t itf_num, usbd_ctrl_xfer_cbs_t *cbs);
bool usbd_unregister_request_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest);
bool usbd_unregister_interface_cb(usbd_handle_t *h, uint8_t itf_num);
bool usbd_endp_open(usbd_handle_t *h, const usb_desc_endpoint_t *ep_desc);
bool usbd_endp_close(usbd_handle_t *h, uint8_t ep_addr);

#ifdef __cplusplus
}
#endif

#endif // USBD_DRIVER_PRIVATE_H
