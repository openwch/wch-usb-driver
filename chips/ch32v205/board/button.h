/**
 * @file button.h
 * @author Links (lhd@wch.cn)
 * @brief Button driver board support package
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef BUTTON_H
#define BUTTON_H

/* @include */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define BUTTON_0 (1 << 0)
#define BUTTON_1 (1 << 1)
#define BUTTON_2 (1 << 2)
#define BUTTON_3 (1 << 3)
#define BUTTON_4 (1 << 4)
#define BUTTON_5 (1 << 5)
#define BUTTON_6 (1 << 6)
#define BUTTON_7 (1 << 7)

/* @function declaration */
void button_init(void);
uint8_t button_read(void);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_H
