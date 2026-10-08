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
#define USB_COUNT 2
#endif

/* @function declaration */
void board_init(void);
usbd_handle_t *board_usbd_init(uint8_t index);
usbd_handle_t *board_usbd_deinit(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif
