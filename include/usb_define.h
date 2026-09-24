/**
 * @file usb_define.h
 * @author Links (lhd@wch.cn)
 * @brief USB driver define header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USB_DEFINE_H
#define USB_DEFINE_H

/* @include */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define USB_ENDP_DIR(ep)    ((ep) & 0x80)
#define USB_ENDP_NUM(ep)    ((ep) & 0x0F)
#define USB_MAX_ENDP_NUM    (16)

/* @enum */
typedef enum
{
    USB_SPEED_UNKNOWN,
    USB_SPEED_FULL,
    USB_SPEED_LOW,
    USB_SPEED_HIGH,
    USB_SPEED_SUPER,
} usb_speed_t;

typedef enum
{
    USB_ENDP_TYPE_CTRL,
    USB_ENDP_TYPE_ISOC,
    USB_ENDP_TYPE_BULK,
    USB_ENDP_TYPE_INTR,
} usb_endp_type_t;

typedef enum
{
    USB_TEST_MODE_RESERVED,
    USB_TEST_MODE_J,
    USB_TEST_MODE_K,
    USB_TEST_MODE_SE0_NAK,
    USB_TEST_MODE_PACKET,
    USB_TEST_MODE_FORCE_ENABLE,
} usb_test_mode_t;

/* @typedef */
typedef uint8_t usb_endp_t;

typedef struct
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

#ifdef __cplusplus
}
#endif

#endif // USB_DEFINE_H
