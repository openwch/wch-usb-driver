/**
 * @file board.h
 * @author Links (lhd@wch.cn)
 * @brief Board support package
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef BOARD_H
#define BOARD_H

/* @include */
#include "usb_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#ifndef USB_COUNT
#define USB_COUNT 1
#endif

/* @enum */
typedef enum
{
    USB_MODE_IDLE,
    USB_MODE_DEVICE,
    USB_MODE_HOST,
} usb_mode_t;

/* @function declaration */
void board_init(void);
void *board_usb_init(uint8_t index, usb_mode_t mode);
void board_usb_deinit(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif
