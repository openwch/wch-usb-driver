/**
 * @file led.h
 * @author Links (lhd@wch.cn)
 * @brief LED driver board support package
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef LED_H
#define LED_H

/* @include */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define LED_0 (1 << 0)
#define LED_1 (1 << 1)
#define LED_2 (1 << 2)

/* @function declaration */
void led_init(void);
void led_write(uint8_t data);

#ifdef __cplusplus
}
#endif

#endif // LED_H
