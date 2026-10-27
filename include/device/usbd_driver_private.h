/********************************** (C) COPYRIGHT *******************************
* File Name          : usbd_driver_private.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB device core private header file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#ifndef USBD_DRIVER_PRIVATE_H
#define USBD_DRIVER_PRIVATE_H

/* @include */
#include "usb_define.h"
#include "device/usbd_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#ifndef USBD_DEV_REQ_CB_COUNT
#define USBD_DEV_REQ_CB_COUNT 12
#endif

#ifndef USBD_ITF_REQ_CB_COUNT
#define USBD_ITF_REQ_CB_COUNT 2
#endif

#ifndef USBD_ENDP_REQ_CB_COUNT
#define USBD_ENDP_REQ_CB_COUNT 2
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

/**
 * @brief USB device data transfer callback function
 * 
 * @param handle USB device driver handle
 * @param endp Endpoint number
 * @param buf Data buffer
 * @param len Length of the data buffer
 */
typedef void (*usbd_data_xfer_cb)(void *handle, usb_endp_t endp, void *buf, size_t len);

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
    bool (*setup)(void *handle, const usb_setup_t *setup, void **buf, size_t *len);
    bool (*data)(void *handle, const usb_setup_t *setup, void *buf, size_t len);
    void (*status)(void *handle, const usb_setup_t *setup, void *buf, size_t len);
} usbd_ctrl_xfer_cbs_t;

typedef struct
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    usbd_ctrl_xfer_cbs_t cbs;
} usbd_dev_req_cb_t;

typedef struct
{
    void *class_handle;
    usbd_ctrl_xfer_cbs_t cbs;
} usbd_itf_req_cb_t;

typedef struct
{
    usb_endp_t endp;
    void *class_handle;
    usbd_ctrl_xfer_cbs_t cbs;
} usbd_endp_req_cb_t;

typedef struct
{
    void *class_handle;
    usbd_data_xfer_cb cb;
} usbd_data_xfer_cbs_t;

typedef struct usbd_handle
{
    /* Port context */
    void *port_ctx;

    /* System context */
    bool remote_wakeup;
    uint8_t ep0_mps;
    uint8_t link_speed;
    uint8_t config_num;
    uint32_t stand_req_buf;
    usbd_get_desc_cb get_desc_cb;
    usbd_event_cb event_cbs[USBD_EVENT_COUNT];

    /* Control transfer context */
    __attribute__((aligned(4))) usb_setup_t setup;
    void *ctrl_handle;
    bool ctrl_xfer_zlp;
    void *ctrl_xfer_buf;
    size_t ctrl_xfer_len;
    const usbd_ctrl_xfer_cbs_t *ctrl_cbs;

    /* Control transfer callback context */
    usbd_dev_req_cb_t dev_req_cbs[USBD_DEV_REQ_CB_COUNT];
    usbd_itf_req_cb_t itf_req_cbs[USBD_ITF_REQ_CB_COUNT];
    usbd_endp_req_cb_t endp_req_cbs[USBD_ENDP_REQ_CB_COUNT];

    /* Data transfer callback context */
    usbd_data_xfer_cbs_t xfer_cb_ctxs[2][USB_MAX_ENDP_NUM - 1];

    /* USB device port operations */
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
    bool (*endp_is_busy)(usbd_handle_t *h, usb_endp_t endp);
    bool (*endp_transfer)(usbd_handle_t *h, usb_endp_t endp, void *buf, size_t len);
} usbd_handle_t;

/* @function declarations */

/**
 * @brief Handle a USB device port event
 * 
 * @param h USB device driver handle
 * @param ctx Context of the USB device port event
 */
void usbd_event_handle(usbd_handle_t *h, usbd_port_event_ctx_t *ctx);

/**
 * @brief Register a device request callback for the USB device driver
 * 
 * @param h USB device driver handle
 * @param bmRequestType bmRequestType of the USB setup request
 * @param bRequest bRequest of the USB setup request
 * @param cbs Callback structure for handling the request
 * 
 * @return Operation result
 */
bool usbd_register_dev_req_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest, usbd_ctrl_xfer_cbs_t *cbs);

/**
 * @brief Register an interface request callback for the USB device driver
 * 
 * @param h USB device driver handle
 * @param class_handle Class handle associated with the interface
 * @param itf_num Interface number
 * @param cbs Callback structure for handling the interface
 * 
 * @return Operation result
 */
bool usbd_register_itf_req_cb(usbd_handle_t *h, void *class_handle, uint8_t itf_num, usbd_ctrl_xfer_cbs_t *cbs);

/**
 * @brief Register an endpoint request callback for the USB device driver
 * 
 * @param h USB device driver handle
 * @param class_handle Class handle associated with the endpoint
 * @param endp Endpoint number
 * @param cbs Callback structure for handling the endpoint request
 * 
 * @return Operation result
 */
bool usbd_register_endp_req_cb(usbd_handle_t *h, void *class_handle, usb_endp_t endp, usbd_ctrl_xfer_cbs_t *cbs);

/**
 * @brief Unregister a device request callback for the USB device driver
 * 
 * @param h USB device driver handle
 * @param bmRequestType bmRequestType of the USB setup request
 * @param bRequest bRequest of the USB setup request
 * 
 * @return Operation result
 */
bool usbd_unregister_dev_req_cb(usbd_handle_t *h, uint8_t bmRequestType, uint8_t bRequest);

/**
 * @brief Unregister an interface request callback for the USB device driver
 * 
 * @param h USB device driver handle
 * @param itf_num Interface number
 * 
 * @return Operation result
 */
bool usbd_unregister_itf_req_cb(usbd_handle_t *h, uint8_t itf_num);

/**
 * @brief Unregister an endpoint request callback for the USB device driver
 * 
 * @param h USB device driver handle
 * @param endp Endpoint number
 * 
 * @return Operation result
 */
bool usbd_unregister_endp_req_cb(usbd_handle_t *h, usb_endp_t endp);

/**
 * @brief Open a USB endpoint for data transfer
 * 
 * @param h USB device driver handle
 * @param class_handle Class handle associated with the endpoint
 * @param ep_desc Endpoint descriptor
 * @param cb Callback function for handling data transfer on the endpoint
 * 
 * @return Operation result
 */
bool usbd_endp_open(usbd_handle_t *h, void *class_handle, const usb_desc_endpoint_t *ep_desc, usbd_data_xfer_cb cb);

/**
 * @brief Close a USB endpoint
 * 
 * @param h USB device driver handle
 * @param endp Endpoint to be closed
 * 
 * @return Operation result
 */
bool usbd_endp_close(usbd_handle_t *h, usb_endp_t endp);

/**
 * @brief Check if a USB endpoint is busy
 * 
 * @param h USB device driver handle
 * @param endp Endpoint to check
 * @return true If the endpoint is busy
 * @return false If the endpoint is not busy
 */
bool usbd_endp_is_busy(usbd_handle_t *h, usb_endp_t endp);

/**
 * @brief Read data from a USB endpoint
 * 
 * @param h USB device driver handle
 * @param endp Endpoint to read data from
 * @param buf Buffer to store the read data
 * @param len Length of the data to read
 * 
 * @return Operation result
 */

bool usbd_endp_read(usbd_handle_t *h, usb_endp_t endp, void *buf, size_t len);

/**
 * @brief Write data to a USB endpoint
 * 
 * @param h USB device driver handle
 * @param endp Endpoint to write data to
 * @param buf Buffer containing the data to write
 * @param len Length of the data to write
 * 
 * @return Operation result
 */
bool usbd_endp_write(usbd_handle_t *h, usb_endp_t endp, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // USBD_DRIVER_PRIVATE_H
