/**
 * @file descriptor.h
 * @author Links (lhd@wch.cn)
 * @brief USB descriptor for CDC-ACM example
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
extern const usb_desc_device_t device_desc;
extern const usb_desc_qualifier_t qua_desc;
extern const uint8_t config_desc_hs[67];
extern const uint8_t config_desc_fs[67];

#ifdef __cplusplus
}
#endif

#endif // DESCRIPTOR_H
