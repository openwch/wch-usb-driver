/**
 * @file cdc_acm.h
 * @author Links (lhd@wch.cn)
 * @brief USB CDC-ACM class definition header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef CDC_ACM_H
#define CDC_ACM_H

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
#define CDC_ACM_CAPBIT_COMM_FEATURE       0x01
#define CDC_ACM_CAPBIT_LINE_CODING        0x02
#define CDC_ACM_CAPBIT_SEND_BREAK         0x04
#define CDC_ACM_CAPBIT_NETWORK_CONNECTION 0x08

#ifdef __cplusplus
}
#endif

#endif // CDC_ACM_H
