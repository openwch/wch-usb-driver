/**
 * @file usb_config.h
 * @author Links (lhd@wch.cn)
 * @brief USB driver configuration header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USB_CONFIG_H
#define USB_CONFIG_H

/* @include */
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// USB driver log output enable macros
#define USB_DRIVER_LOG_INFO_EN
#define USB_DRIVER_LOG_WARNING_EN
#define USB_DRIVER_LOG_ERROR_EN

// USB driver log output interface macro definitions.
#ifndef USB_LOG_OUTPUT
#define USB_LOG_OUTPUT(format, ...) printf(format, ##__VA_ARGS__)
#endif

#define USBD_REQUEST_CB_COUNT   16
#define USBD_INTERFACE_CB_COUNT 8

#ifdef __cplusplus
}
#endif

#endif // USB_CONFIG_H
