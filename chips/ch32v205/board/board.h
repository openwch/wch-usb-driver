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

/* @function declaration */
void board_init(void);
usbd_handle_t *board_usbd_init(uint8_t dev_id);
usbd_handle_t *board_usbd_deinit(uint8_t dev_id);

#ifdef __cplusplus
}
#endif

#endif
