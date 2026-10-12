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

/* @define */
#ifndef USBH_DRIVER_MAX_COUNT
#define USBH_DRIVER_MAX_COUNT 4
#endif

#ifndef USBH_DESC_BUF_SIZE
#define USBH_DESC_BUF_SIZE 512
#endif

#define USBH_DRV_MATCH_ALL          0xFFFF
#define USBH_DRV_MATCH_VID          0x0001
#define USBH_DRV_MATCH_PID          0x0002
#define USBH_DRV_MATCH_DEV_CLASS    0x0004
#define USBH_DRV_MATCH_DEV_SUBCLASS 0x0008
#define USBH_DRV_MATCH_DEV_PROTOCOL 0x0010
#define USBH_DRV_MATCH_ITF_CLASS    0x0020
#define USBH_DRV_MATCH_ITF_SUBCLASS 0x0040
#define USBH_DRV_MATCH_ITF_PROTOCOL 0x0080

#define USBH_DRV_ALL_INTERFACE      0xFF

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

typedef enum
{
    USBH_PID_OUT = 0x01,
    USBH_PID_IN = 0x09,
    USBH_PID_SOF = 0x05,
    USBH_PID_SETUP = 0x0D,

    USBH_PID_DATA0 = 0x03,
    USBH_PID_DATA1 = 0x0B,
    USBH_PID_DATA2 = 0x07,
    USBH_PID_MDATA = 0x0F,

    USBH_PID_ACK = 0x02,
    USBH_PID_NAK = 0x0A,
    USBH_PID_STALL = 0x0E,
    USBH_PID_NYET = 0x06,

    USBH_PID_PRE_ERR = 0x0C,
    USBH_PID_SPLIT = 0x08,
    USBH_PID_PING = 0x04,
    USBH_PID_RESERVED = 0x00,
} usbh_pid_t;

typedef enum
{
    USBH_TOGGLE_DATA0,
    USBH_TOGGLE_DATA1,
    USBH_TOGGLE_DATA2,
    USBH_TOGGLE_MDATA,
} usbh_toggle_t;

/* @function pointer */
typedef void (*usbh_ctrl_xfer_cb)(void *handle, bool rst, const usb_setup_t *setup, const void *buf, uint16_t length);
typedef void (*usbh_data_xfer_cb)(void *handle, bool rst, usb_endp_t endp, const void *buf, size_t len);
typedef bool (*usbh_drv_bind_cb)(usbh_device_t *dev, const void *desc, uint16_t length);
typedef bool (*usbh_drv_unbind_cb)(usbh_device_t *dev, uint8_t itf_num);
typedef void (*usbh_drv_dev_wakeup_cb)(usbh_device_t *dev);

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

    /* Transfer unit context */
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

    /* Associated endpoint */
    usbh_endpoint_t *endpoint;
} usbh_xfer_unit_t;

typedef struct usbh_xfer_ctx
{
    bool is_busy;
    bool is_stalled;
    uint8_t toggle;
    uint8_t ctrl_stage;
    uint32_t tick;
    uint32_t length;
    uint32_t offset;
    uint32_t timestamp;
    uint32_t timeout;
    void *buf;
    const usb_setup_t *setup;
} usbh_xfer_ctx_t;

typedef struct usbh_endpoint
{
    struct usbh_endpoint *next;
    bool is_used;
    bool ping_en;
    uint8_t type;
    uint8_t addr;
    uint16_t mps;
    uint16_t interval;
    void *handle;
    void *xfer_cb;
    usbh_xfer_ctx_t xfer_ctx;
    usbh_xfer_unit_t xfer_unit;
} usbh_endpoint_t;

typedef struct usbh_device
{
    struct usbh_device *next;

    /* Device handle context */
    usbh_handle_t *host;
    bool self_powered;
    bool remote_wakeup;
    bool suspended;
    uint8_t address;
    uint8_t hub_addr;
    uint8_t hub_port;
    uint8_t enum_stage;
    uint8_t speed;
    uint8_t suspend_stage;
    uint16_t language_id;
    usbh_drv_dev_wakeup_cb wakeup_cb;

    /* Endpoint context */
    usbh_endpoint_t ctrl_endp;
    usbh_endpoint_t *data_endp[2][USB_MAX_ENDP_NUM - 1];

    /* Device descriptor */
    usb_desc_device_t dev_desc;
} usbh_device_t;

typedef struct
{
    /* VID PID information */
    uint16_t vid;
    uint16_t pid;

    /* Device information */
    uint8_t dev_class;
    uint8_t dev_subclass;
    uint8_t dev_protocol;

    /* Interface information */
    uint8_t itf_class;
    uint8_t itf_subclass;
    uint8_t itf_protocol;

    /* Match criteria */
    uint16_t match_bitmap;

    /* Driver callbacks */
    usbh_drv_bind_cb bind;
    usbh_drv_unbind_cb unbind;
} usbh_drv_info_t;

typedef struct usbh_handle
{
    /* Port context */
    void *port_ctx;

    /* System context */
    bool root_port_change;
    bool xfer_busy;
    uint8_t speed;
    uint32_t bus_address_bitmap[4];
    uint32_t tick;
    usbh_device_t *device_list;
    const usbh_drv_info_t *drv_info_list[USBH_DRIVER_MAX_COUNT];

    /* Transfer context */
    uint32_t xfer_tick;
    usbh_xfer_unit_t *xfer_unit_list;
    usbh_endpoint_t *endpoint_list[4];

    /* Enumeration descriptor buffer */
    __attribute__((aligned(4))) usb_setup_t ctrl_setup;
    __attribute__((aligned(4))) uint8_t enum_desc_buf[USBH_DESC_BUF_SIZE];

    /* USB host port operations */
    bool (*open)(usbh_handle_t *h);
    bool (*close)(usbh_handle_t *h);
    void (*start_transfer)(usbh_handle_t *h);

    void (*root_get_status)(usbh_handle_t *h, usbh_port_status_t *status);
    void (*root_set_feature)(usbh_handle_t *h, usbh_port_feature_t feature);

    /* USB host hub operations */
    void (*port_get_status)(usbh_device_t *dev, usbh_port_status_t *status);
    void (*port_set_feature)(usbh_device_t *dev, usbh_port_feature_t feature);
} usbh_handle_t;

/* @function declaration */
void usbh_device_insert(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port);
void usbh_device_remove(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port);
void usbh_device_reset(usbh_device_t *dev);
void usbh_device_suspend(usbh_device_t *dev, usbh_drv_dev_wakeup_cb wakeup_cb);
void usbh_device_resume(usbh_handle_t *h, uint8_t hub_addr, uint8_t hub_port);
bool usbh_register_driver(usbh_handle_t *h, const usbh_drv_info_t *drv_info);
bool usbh_unregister_driver(usbh_handle_t *h, const usbh_drv_info_t *drv_info);
bool usbh_endp_open(usbh_device_t *dev, void *handle, const usb_desc_endpoint_t *ep_desc, usbh_data_xfer_cb cb);
bool usbh_endp_close(usbh_device_t *dev, usb_endp_t endp);
bool usbh_endp_read(usbh_device_t *dev, usb_endp_t endp, void *buf, size_t len, uint32_t timeout);
bool usbh_endp_write(usbh_device_t *dev, usb_endp_t endp, const void *buf, size_t len, uint32_t timeout);
bool usbh_ctrl_xfer(usbh_device_t *dev, void *handle, const usb_setup_t *setup, void *buf, uint32_t timeout,
                    usbh_ctrl_xfer_cb cb);
bool usbh_is_ctrl_xfering(usbh_device_t *dev);
bool usbh_is_endp_xfering(usbh_device_t *dev, usb_endp_t endp);
uint32_t usbh_drv_get_timestamp(usbh_handle_t *h);

/* @inline functions */
static inline void usbh_list_append(void **list, void *node)
{
    while (*list != NULL && *list != node)
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
