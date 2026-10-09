/**
 * @file usbfsh_port.c
 * @author Links (lhd@wch.cn)
 * @brief USB Full-Speed host port source file
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include "usb_driver.h"
#include "usbfs_port.h"

/* @define */
#define USBFSH ((usbfsh_ip_t *)h->base_addr)

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

void usbfsh_handle_init(usbh_handle_t *h, uint32_t base_addr, usbfsh_ctx_t *ctx)
{
    memset(h, 0, sizeof(usbh_handle_t));
    h->base_addr = base_addr;
    h->port_ctx = ctx;

    h->open = open;
    h->close = close;
}

void usbfsh_event_handle(usbh_handle_t *h)
{
    uint8_t int_flag = USBFSH->INT_FG;
    USBFSH->INT_FG = int_flag;
}
