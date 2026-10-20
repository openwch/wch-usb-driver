/********************************** (C) COPYRIGHT *******************************
* File Name          : usbfsh_port.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2026/10/20
* Description        : USB Full-Speed host port source file
*********************************************************************************
* Copyright (c) 2026 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/* @include */
#include "usb_driver.h"
#include "usbfs_port.h"

/* @define */
#define USBFSH ((usbfsh_ip_t *)((usbfsh_ctx_t *)h->port_ctx)->base_addr)

static void unit_transfer(usbh_handle_t *h, usbh_xfer_unit_t *xfer)
{
    USBFSH->DEV_ADDR = xfer->dev_addr;
    if (xfer->pre)
    {
        USBFSH->BASE_CTRL |= USBFS_UC_LOW_SPEED;
        USBFSH->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
    }
    else
    {
        USBFSH->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
        USBFSH->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
    }

    if (xfer->token == USBH_PID_IN)
    {
        USBFSH->HOST_RX_DMA = (uint32_t)xfer->buf;
        USBFSH->HOST_RX_CTRL = (xfer->toggle == USBH_TOGGLE_DATA1 ? USBFS_UH_R_TOG : 0) |
                               (xfer->iso ? USBFS_UH_R_RES : 0);
        USBFSH->HOST_EP_PID = (xfer->token << 4) | xfer->endp_num;
    }
    else
    {
        USBFSH->HOST_TX_LEN = xfer->xfer_len;
        USBFSH->HOST_TX_DMA = (uint32_t)xfer->buf;
        USBFSH->HOST_TX_CTRL = (xfer->toggle == USBH_TOGGLE_DATA1 ? USBFS_UH_T_TOG : 0) |
                               (xfer->iso ? USBFS_UH_T_RES : 0);
        USBFSH->HOST_EP_PID = (xfer->token << 4) | xfer->endp_num;
    }
}

static bool open(usbh_handle_t *h)
{
    USBFSH->BASE_CTRL = USBFS_UC_HOST_MODE;
    while (!(USBFSH->BASE_CTRL & USBFS_UC_HOST_MODE));
    USBFSH->HOST_CTRL = 0;
    USBFSH->DEV_ADDR = 0;
    USBFSH->HOST_EP_MOD = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;
    USBFSH->HOST_SETUP = USBFS_UH_SOF_EN;

    USBFSH->HOST_RX_CTRL = 0;
    USBFSH->HOST_TX_CTRL = 0;
    USBFSH->BASE_CTRL = USBFS_UC_HOST_MODE | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;

    USBFSH->INT_FG = 0xFF;
    USBFSH->INT_EN = USBFS_UIE_HST_SOF | USBFS_UIE_SUSPEND | USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
    return true;
}

static bool close(usbh_handle_t *h)
{
    usbfsh_ctx_t *port_ctx = (usbfsh_ctx_t *)h->port_ctx;
    USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    port_ctx->delay_us(10);
    USBFSH->BASE_CTRL = 0;
    return true;
}

static void start_transfer(usbh_handle_t *h)
{
    usbfsh_ctx_t *ctx = (usbfsh_ctx_t *)h->port_ctx;
    ctx->xfer_list = h->xfer_unit_list;
    unit_transfer(h, ctx->xfer_list);
}

static void root_set_feature(usbh_handle_t *h, usbh_port_feature_t feature)
{
    switch (feature)
    {
    case USBH_PORT_FEATURE_RESET:
        USBFSH->INT_EN = 0;
        USBFSH->HOST_CTRL = USBFS_UH_BUS_RESET;
        ((usbfsh_ctx_t *)h->port_ctx)->delay_ms(12);
        USBFSH->HOST_CTRL = 0;
        ((usbfsh_ctx_t *)h->port_ctx)->delay_ms(2);
        USBFSH->INT_FG = 0xFF;
        USBFSH->INT_EN = USBFS_UIE_HST_SOF | USBFS_UIE_SUSPEND | USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
        USBFSH->HOST_CTRL = USBFS_UH_PORT_EN;
        break;

    case USBH_PORT_FEATURE_SUSPEND:
        USBFSH->HOST_CTRL = 0;
        break;
    }
}

static void root_get_status(usbh_handle_t *h, usbh_port_status_t *status)
{
    status->status = 0;

    uint16_t port_status = USBFSH->MIS_ST;
    status->status_bits.power = 1;
    if (port_status & USBFS_UMS_DEV_ATTACH) status->status_bits.connect = 1;
    if (USBFSH->HOST_CTRL & USBFS_UH_PORT_EN) status->status_bits.enable = 1;
    if (port_status & USBFS_UMS_SUSPEND) status->status_bits.suspend = 1;
    if (port_status & USBFS_UMS_BUS_RESET) status->status_bits.reset = 1;
    if (port_status & USBFS_UMS_DM_LEVEL) status->status_bits.low_speed = 1;
}

void usbfsh_handle_init(usbh_handle_t *h, usbfsh_ctx_t *ctx)
{
    memset(h, 0, sizeof(usbh_handle_t));
    h->port_ctx = ctx;

    h->open = open;
    h->close = close;
    h->start_transfer = start_transfer;
    h->root_set_feature = root_set_feature;
    h->root_get_status = root_get_status;
}

void usbfsh_event_handle(usbh_handle_t *h)
{
    uint8_t int_flag = USBFSH->INT_FG;

    if (int_flag & USBFS_UIF_TRANSFER)
    {
        USBFSH->HOST_EP_PID = 0x00;
        USBFSH->INT_FG = USBFS_UIF_TRANSFER;

        usbfsh_ctx_t *ctx = (usbfsh_ctx_t *)h->port_ctx;
        if (USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH)
        {
            if (ctx->xfer_list->token == USBH_PID_IN)
            {
                ctx->xfer_list->xfer_len = USBFSH->RX_LEN;
            }
            ctx->xfer_list->rx_pid = USBFSH->INT_ST & 0x0F;
            ctx->xfer_list = ctx->xfer_list->next;
        }
        else
        {
            ctx->xfer_list = NULL;
        }

        if (ctx->xfer_list)
        {
            unit_transfer(h, ctx->xfer_list);
        }
        else
        {
            h->xfer_busy = false;
        }
    }
    else if (int_flag & USBFS_UIF_HST_SOF)
    {
        USBFSH->INT_FG = USBFS_UIF_HST_SOF;
        h->tick++;
    }
    else if (int_flag & USBFS_UIF_DETECT)
    {
        USBFSH->INT_FG = USBFS_UIF_DETECT;
        h->root_port_change = true;
    }
    else if (int_flag & USBFS_UIF_SUSPEND)
    {
        USBFSH->INT_FG = USBFS_UIF_SUSPEND;
        if ((USBFSH->MIS_ST & USBFS_UMS_SUSPEND) == 0)
        {
            USBFSH->HOST_CTRL = USBFS_UH_PORT_EN;
            usbh_device_resume(h, 0, 0);
        }
    }
    else
    {
        USBFSH->INT_FG = int_flag;
    }
}
