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

typedef struct usbh_handle
{
    /* Port context */
    void *port_ctx;

    bool root_port_change;

    /* USB host port operations */
    bool (*open)(usbh_handle_t *h);
    bool (*close)(usbh_handle_t *h);
    void (*start_transfer)(usbh_handle_t *h);

    void (*root_get_status)(usbh_handle_t *h, usbh_port_status_t *status);
    void (*root_set_feature)(usbh_handle_t *h, usbh_port_feature_t feature);
} usbh_handle_t;

#ifdef __cplusplus
}
#endif

#endif // USBH_DRIVER_PRIVATE_H
