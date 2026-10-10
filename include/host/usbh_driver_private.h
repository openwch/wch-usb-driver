/**
 * @file usbh_driver_private.h
 * @author Links (lhd@wch.cn)
 * @brief USB host core private header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USBH_DRIVER_PRIVATE_H
#define USBH_DRIVER_PRIVATE_H

/* @include */
#include "usb_define.h"
#include "host/usbh_driver_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @enum */
typedef enum
{
    USBH_PORT_FEATURE_CONNECTION = 0x00,
    USBH_PORT_FEATURE_ENABLE = 0x01,
    USBH_PORT_FEATURE_SUSPEND = 0x02,
    USBH_PORT_FEATURE_OVER_CURRENT = 0x03,
    USBH_PORT_FEATURE_RESET = 0x04,
    USBH_PORT_FEATURE_POWER = 0x08,
    USBH_PORT_FEATURE_LOW_SPEED = 0x09,
    USBH_PORT_FEATURE_TEST = 0x15,
    USBH_PORT_FEATURE_INDICATOR = 0x16,
} usbh_port_feature_t;

/* @function pointer */
typedef void (*usbh_data_xfer_cb)(void *handle, usb_endp_t endp, void *buf, size_t len);

/* @typedef */
typedef struct usbh_endpoint usbh_endpoint_t;

/* @struct */
typedef struct __attribute__((packed))
{
    union
    {
        uint16_t status;

        struct
        {
            uint16_t connect : 1;
            uint16_t enable : 1;
            uint16_t suspend : 1;
            uint16_t over_current : 1;
            uint16_t reset : 1;
            uint16_t reserved1 : 3;
            uint16_t power : 1;
            uint16_t low_speed : 1;
            uint16_t high_speed : 1;
            uint16_t test_mode : 1;
            uint16_t port_indicator : 1;
            uint16_t reserved2 : 3;
        } status_bits;
    };
} usbh_port_status_t;

typedef struct
{
    union
    {
        uint32_t split_data;

        struct
        {
            uint32_t hub_addr : 7;
            uint32_t sc : 1;
            uint32_t port : 7;
            uint32_t s : 1;
            uint32_t e : 1;
            uint32_t et : 2;
            uint32_t reserved : 13;
        };
    };
} usbh_split_data_t;

typedef struct usbh_xfer_unit
{
    struct usbh_xfer_unit *next;
    usbh_endpoint_t *endpoint;

    uint32_t xfer_len;
    void *buf;
    bool pre;
    bool iso;
    uint8_t dev_addr;
    uint8_t token;
    uint8_t endp_num;
    uint8_t toggle;
    uint8_t rx_pid;

    /** Split transfer context */
    uint8_t split_retry;
    uint32_t split_tick;
    uint16_t split_iso_out_len;
    uint16_t split_iso_out_ofs;
    usbh_split_data_t split_data;
} usbh_xfer_unit_t;

typedef struct usbh_xfer_ctx
{
    bool is_busy;
    bool is_stalled;
    uint8_t toggle;
    uint8_t ctrl_stage;
    uint32_t tick;
    uint32_t retry;
    uint32_t length;
    uint32_t offset;
    usb_request_t *request;
    void *buf;
} usbh_xfer_ctx_t;

typedef struct usbh_endpoint
{
    struct usbh_endpoint *next;
    bool ping_en;
    uint8_t type;
    uint8_t addr;
    uint16_t mps;
    uint16_t interval;
    void *class_handle;
    void *xfer_cb;
    usbh_xfer_ctx_t xfer_ctx;
    usbh_xfer_unit_t xfer_unit;
} usbh_endpoint_t;

typedef struct usbh_device
{
    struct usbh_device *next;
    uint8_t address;
    uint8_t hub_addr;
    uint8_t hub_port;
    uint8_t enum_stage;
    uint8_t speed;
    uint8_t ep0_mps;
    usbh_endpoint_t ctrl_endp;
    usbh_endpoint_t *data_endp[2][USB_MAX_ENDP_NUM - 1];
} usbh_device_t;

typedef struct usbh_handle
{
    /* Port context */
    void *port_ctx;

    /* Root port change flag */
    bool root_port_change;

    /* USB host speed */
    uint8_t speed;

    /* Bus address bitmap */
    uint32_t bus_address_bitmap[4];

    /* System tick */
    uint32_t tick;

    /* Device list */
    usbh_device_t *device_list;

    /* Endpoint list */
    usbh_endpoint_t *endpoint_list[4];

    /* USB host port operations */
    bool (*open)(usbh_handle_t *h);
    bool (*close)(usbh_handle_t *h);
    void (*start_transfer)(usbh_handle_t *h);

    void (*root_get_status)(usbh_handle_t *h, usbh_port_status_t *status);
    void (*root_set_feature)(usbh_handle_t *h, usbh_port_feature_t feature);

    /* USB host hub operations */
    void (*port_get_status)(usbh_handle_t *h, usbh_port_status_t *status);
    void (*port_set_feature)(usbh_handle_t *h, usbh_port_feature_t feature);
} usbh_handle_t;

/* @function declaration */
void usbh_device_insert(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port);
void usbh_device_remove(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port);
void usbh_device_reset(usbh_handle_t *h, usbh_device_t *dev);
bool usbh_endp_open(usbh_handle_t *h, const void *class, const usb_desc_endpoint_t *ep_desc, usbh_data_xfer_cb cb);
bool usbh_endp_close(usbh_handle_t *h, usb_endp_t endp);
bool usbh_endp_read(usbh_handle_t *h, usb_endp_t endp, void *buf, size_t len);
bool usbh_endp_write(usbh_handle_t *h, usb_endp_t endp, const void *buf, size_t len);

/* @inline functions */
static inline void usbh_list_append(void **list, void *node)
{
    while (*list != NULL)
    {
        list = (void **)*list;
    }
    *list = node;
}

static inline void usbh_list_remove(void **list, void *node)
{
    while (*list != NULL)
    {
        if (*list == node)
        {
            *list = *((void **)node);
            return;
        }
        list = (void **)*list;
    }
}

#ifdef __cplusplus
}
#endif

#endif // USBH_DRIVER_PRIVATE_H
