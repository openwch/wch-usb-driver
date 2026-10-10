/**
 * @file usbfs_port.h
 * @author Links (lhd@wch.cn)
 * @brief USB Full-Speed port header file
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef USBFS_PORT_H
#define USBFS_PORT_H

/* @include */
#include <stdint.h>

#include "device/usbd_driver_private.h"
#include "host/usbh_driver_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @define */
/* R8_USB_CTRL */
#define USBFS_UC_HOST_MODE     0x80
#define USBFS_UC_LOW_SPEED     0x40
#define USBFS_UC_DEV_PU_EN     0x20
#define USBFS_UC_SYS_CTRL_MASK 0x30
#define USBFS_UC_SYS_CTRL0     0x00
#define USBFS_UC_SYS_CTRL1     0x10
#define USBFS_UC_SYS_CTRL2     0x20
#define USBFS_UC_SYS_CTRL3     0x30
#define USBFS_UC_INT_BUSY      0x08
#define USBFS_UC_RESET_SIE     0x04
#define USBFS_UC_CLR_ALL       0x02
#define USBFS_UC_DMA_EN        0x01

/* R8_USB_INT_EN */
#define USBFS_UIE_DEV_SOF      0x80
#define USBFS_UIE_DEV_NAK      0x40
#define USBFS_U_1WIRE_MODE     0x20
#define USBFS_UIE_FIFO_OV      0x10
#define USBFS_UIE_HST_SOF      0x08
#define USBFS_UIE_SUSPEND      0x04
#define USBFS_UIE_TRANSFER     0x02
#define USBFS_UIE_DETECT       0x01
#define USBFS_UIE_BUS_RST      0x01

/* R8_USB_DEV_AD */
#define USBFS_UDA_GP_BIT       0x80
#define USBFS_USB_ADDR_MASK    0x7F

/* R8_USB_MIS_ST */
#define USBFS_UMS_SOF_PRES     0x80
#define USBFS_UMS_SOF_ACT      0x40
#define USBFS_UMS_SIE_FREE     0x20
#define USBFS_UMS_R_FIFO_RDY   0x10
#define USBFS_UMS_BUS_RESET    0x08
#define USBFS_UMS_SUSPEND      0x04
#define USBFS_UMS_DM_LEVEL     0x02
#define USBFS_UMS_DEV_ATTACH   0x01

/* R8_USB_INT_FG */
#define USBFS_U_IS_NAK         0x80
#define USBFS_U_TOG_OK         0x40
#define USBFS_U_SIE_FREE       0x20
#define USBFS_UIF_FIFO_OV      0x10
#define USBFS_UIF_HST_SOF      0x08
#define USBFS_UIF_SUSPEND      0x04
#define USBFS_UIF_TRANSFER     0x02
#define USBFS_UIF_DETECT       0x01
#define USBFS_UIF_BUS_RST      0x01

/* R8_USB_INT_ST */
#define USBFS_UIS_IS_NAK       0x80
#define USBFS_UIS_TOG_OK       0x40
#define USBFS_UIS_TOKEN_MASK   0x30
#define USBFS_UIS_TOKEN_OUT    0x00
#define USBFS_UIS_TOKEN_SOF    0x10
#define USBFS_UIS_TOKEN_IN     0x20
#define USBFS_UIS_TOKEN_SETUP  0x30
#define USBFS_UIS_ENDP_MASK    0x0F
#define USBFS_UIS_H_RES_MASK   0x0F

/* R32_USB_OTG_CR */
#define USBFS_CR_SESS_VTH      0x20
#define USBFS_CR_VBUS_VTH      0x10
#define USBFS_CR_OTG_EN        0x08
#define USBFS_CR_IDPU          0x04
#define USBFS_CR_CHARGE_VBUS   0x02
#define USBFS_CR_DISCHAR_VBUS  0x01

/* R32_USB_OTG_SR */
#define USBFS_SR_ID_DIG        0x08
#define USBFS_SR_SESS_END      0x04
#define USBFS_SR_SESS_VLD      0x02
#define USBFS_SR_VBUS_VLD      0x01

/* R8_UDEV_CTRL */
#define USBFS_UD_PD_DIS        0x80
#define USBFS_UD_DP_PIN        0x20
#define USBFS_UD_DM_PIN        0x10
#define USBFS_UD_LOW_SPEED     0x04
#define USBFS_UD_GP_BIT        0x02
#define USBFS_UD_PORT_EN       0x01

/* R8_UEP4_1_MOD */
#define USBFS_UEP1_RX_EN       0x80
#define USBFS_UEP1_TX_EN       0x40
#define USBFS_UEP1_BUF_MOD     0x10
#define USBFS_UEP4_RX_EN       0x08
#define USBFS_UEP4_TX_EN       0x04
#define USBFS_UEP4_BUF_MOD     0x01

/* R8_UEP2_3_MOD */
#define USBFS_UEP3_RX_EN       0x80
#define USBFS_UEP3_TX_EN       0x40
#define USBFS_UEP3_BUF_MOD     0x10
#define USBFS_UEP2_RX_EN       0x08
#define USBFS_UEP2_TX_EN       0x04
#define USBFS_UEP2_BUF_MOD     0x01

/* R8_UEP5_6_MOD */
#define USBFS_UEP6_RX_EN       0x80
#define USBFS_UEP6_TX_EN       0x40
#define USBFS_UEP6_BUF_MOD     0x10
#define USBFS_UEP5_RX_EN       0x08
#define USBFS_UEP5_TX_EN       0x04
#define USBFS_UEP5_BUF_MOD     0x01

/* R8_UEP7_MOD */
#define USBFS_UEP7_RX_EN       0x08
#define USBFS_UEP7_TX_EN       0x04
#define USBFS_UEP7_BUF_MOD     0x01

/* R8_UEPn_TX_CTRL */
#define USBFS_UEP_T_AUTO_TOG   0x08
#define USBFS_UEP_T_TOG        0x04
#define USBFS_UEP_T_RES_MASK   0x03
#define USBFS_UEP_T_RES_ACK    0x00
#define USBFS_UEP_T_RES_NONE   0x01
#define USBFS_UEP_T_RES_NAK    0x02
#define USBFS_UEP_T_RES_STALL  0x03

/* R8_UEPn_RX_CTRL, n=0-7 */
#define USBFS_UEP_R_AUTO_TOG   0x08
#define USBFS_UEP_R_TOG        0x04
#define USBFS_UEP_R_RES_MASK   0x03
#define USBFS_UEP_R_RES_ACK    0x00
#define USBFS_UEP_R_RES_NONE   0x01
#define USBFS_UEP_R_RES_NAK    0x02
#define USBFS_UEP_R_RES_STALL  0x03

/* R8_UHOST_CTRL */
#define USBFS_UH_PD_DIS        0x80
#define USBFS_UH_DP_PIN        0x20
#define USBFS_UH_DM_PIN        0x10
#define USBFS_UH_LOW_SPEED     0x04
#define USBFS_UH_BUS_RESET     0x02
#define USBFS_UH_PORT_EN       0x01

/* R32_UH_EP_MOD */
#define USBFS_UH_EP_TX_EN      0x40
#define USBFS_UH_EP_TBUF_MOD   0x10

#define USBFS_UH_EP_RX_EN      0x08
#define USBFS_UH_EP_RBUF_MOD   0x01

/* R16_UH_SETUP */
#define USBFS_UH_PRE_PID_EN    0x0400
#define USBFS_UH_SOF_EN        0x0004

/* R8_UH_EP_PID */
#define USBFS_UH_TOKEN_MASK    0xF0
#define USBFS_UH_ENDP_MASK     0x0F

/* R8_UH_RX_CTRL */
#define USBFS_UH_R_AUTO_TOG    0x08
#define USBFS_UH_R_TOG         0x04
#define USBFS_UH_R_RES         0x01

/* R8_UH_TX_CTRL */
#define USBFS_UH_T_AUTO_TOG    0x08
#define USBFS_UH_T_TOG         0x04
#define USBFS_UH_T_RES         0x01

#ifdef __cplusplus
#define __I volatile
#else
#define __I volatile const
#endif

#define __O  volatile
#define __IO volatile

/* @struct */
typedef struct
{
    __IO uint8_t BASE_CTRL;
    __IO uint8_t UDEV_CTRL;
    __IO uint8_t INT_EN;
    __IO uint8_t DEV_ADDR;
    __IO uint8_t Reserve0;
    __IO uint8_t MIS_ST;
    __IO uint8_t INT_FG;
    __IO uint8_t INT_ST;
    __IO uint32_t RX_LEN;
    __IO uint8_t UEP4_1_MOD;
    __IO uint8_t UEP2_3_MOD;
    __IO uint8_t UEP5_6_MOD;
    __IO uint8_t UEP7_MOD;
    __IO uint32_t UEP0_DMA;
    __IO uint32_t UEP1_DMA;
    __IO uint32_t UEP2_DMA;
    __IO uint32_t UEP3_DMA;
    __IO uint32_t UEP4_DMA;
    __IO uint32_t UEP5_DMA;
    __IO uint32_t UEP6_DMA;
    __IO uint32_t UEP7_DMA;
    __IO uint16_t UEP0_TX_LEN;

    union
    {
        __IO uint16_t UEP0_CTRL;

        struct
        {
            __IO uint8_t UEP0_TX_CTRL;
            __IO uint8_t UEP0_RX_CTRL;
        };
    };

    __IO uint16_t UEP1_TX_LEN;

    union
    {
        __IO uint16_t UEP1_CTRL;

        struct
        {
            __IO uint8_t UEP1_TX_CTRL;
            __IO uint8_t UEP1_RX_CTRL;
        };
    };

    __IO uint16_t UEP2_TX_LEN;

    union
    {
        __IO uint16_t UEP2_CTRL;

        struct
        {
            __IO uint8_t UEP2_TX_CTRL;
            __IO uint8_t UEP2_RX_CTRL;
        };
    };

    __IO uint16_t UEP3_TX_LEN;

    union
    {
        __IO uint16_t UEP3_CTRL;

        struct
        {
            __IO uint8_t UEP3_TX_CTRL;
            __IO uint8_t UEP3_RX_CTRL;
        };
    };

    __IO uint16_t UEP4_TX_LEN;

    union
    {
        __IO uint16_t UEP4_CTRL;

        struct
        {
            __IO uint8_t UEP4_TX_CTRL;
            __IO uint8_t UEP4_RX_CTRL;
        };
    };

    __IO uint16_t UEP5_TX_LEN;

    union
    {
        __IO uint16_t UEP5_CTRL;

        struct
        {
            __IO uint8_t UEP5_TX_CTRL;
            __IO uint8_t UEP5_RX_CTRL;
        };
    };

    __IO uint16_t UEP6_TX_LEN;

    union
    {
        __IO uint16_t UEP6_CTRL;

        struct
        {
            __IO uint8_t UEP6_TX_CTRL;
            __IO uint8_t UEP6_RX_CTRL;
        };
    };

    __IO uint16_t UEP7_TX_LEN;

    union
    {
        __IO uint16_t UEP7_CTRL;

        struct
        {
            __IO uint8_t UEP7_TX_CTRL;
            __IO uint8_t UEP7_RX_CTRL;
        };
    };

    __IO uint32_t Reserve1;
    __IO uint32_t OTG_CR;
    __IO uint32_t OTG_SR;
} usbfsd_ip_t;

typedef struct
{
    __IO uint8_t BASE_CTRL;
    __IO uint8_t HOST_CTRL;
    __IO uint8_t INT_EN;
    __IO uint8_t DEV_ADDR;
    __IO uint8_t Reserve0;
    __IO uint8_t MIS_ST;
    __IO uint8_t INT_FG;
    __IO uint8_t INT_ST;
    __IO uint16_t RX_LEN;
    __IO uint16_t Reserve1;
    __IO uint8_t Reserve2;
    __IO uint8_t HOST_EP_MOD;
    __IO uint16_t Reserve3;
    __IO uint32_t Reserve4;
    __IO uint32_t Reserve5;
    __IO uint32_t HOST_RX_DMA;
    __IO uint32_t HOST_TX_DMA;
    __IO uint32_t Reserve6;
    __IO uint32_t Reserve7;
    __IO uint32_t Reserve8;
    __IO uint32_t Reserve9;
    __IO uint32_t Reserve10;
    __IO uint16_t Reserve11;
    __IO uint16_t HOST_SETUP;
    __IO uint8_t HOST_EP_PID;
    __IO uint8_t Reserve12;
    __IO uint8_t Reserve13;
    __IO uint8_t HOST_RX_CTRL;
    __IO uint16_t HOST_TX_LEN;
    __IO uint8_t HOST_TX_CTRL;
    __IO uint8_t Reserve14;
    __IO uint32_t Reserve15;
    __IO uint32_t Reserve16;
    __IO uint32_t Reserve17;
    __IO uint32_t Reserve18;
    __IO uint32_t Reserve19;
    __IO uint32_t OTG_CR;
    __IO uint32_t OTG_SR;
} usbfsh_ip_t;

typedef struct
{
    uint16_t mps;
    void *xfer_buf;
    size_t xfer_len;
    size_t xfer_ofs;
} usbfs_xfer_ctx_t;

typedef struct
{
    uint32_t base_addr;
    usbfs_xfer_ctx_t xfer_ctxs[2][8];
    uint8_t *dma_buf_ptrs[2][8];
    __attribute__((aligned(4))) uint8_t endp_dma_bufs[8][128];
    void (*delay_us)(uint32_t us);
    void (*delay_ms)(uint32_t ms);
} usbfsd_ctx_t;

typedef struct
{
    uint32_t base_addr;
    void (*delay_us)(uint32_t us);
    void (*delay_ms)(uint32_t ms);
} usbfsh_ctx_t;

/* @function declaration */
void usbfsd_handle_init(usbd_handle_t *h, usbfsd_ctx_t *ctx);
void usbfsh_handle_init(usbh_handle_t *h, usbfsh_ctx_t *ctx);
void usbfsd_event_handle(usbd_handle_t *h);
void usbfsh_event_handle(usbh_handle_t *h);

#ifdef __cplusplus
}
#endif

#endif // USBFS_PORT_H
