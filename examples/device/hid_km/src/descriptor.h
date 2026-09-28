/**
 * @file descriptor.h
 * @author Links (lhd@wch.cn)
 * @brief USB descriptor for HID keyboard and mouse example
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H

/* @include */
#include "usb_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @extern */
extern usb_desc_device_t device_desc;
extern usb_desc_qualifier_t qualifier_desc;
extern uint8_t config_desc[59];
extern uint8_t keyboard_report_desc[62];
extern uint8_t mouse_report_desc[52];

#ifdef __cplusplus
}
#endif

#endif // DESCRIPTOR_H
