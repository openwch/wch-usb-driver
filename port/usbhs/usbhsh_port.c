/**
 * @file usbhsh_port.c
 * @author Links (lhd@wch.cn)
 * @brief USB High-Speed host port source file
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include "usb_driver.h"
#include "usbhs_port.h"

/* @define */
#define USBHSH ((usbhsh_ip_t *)h->base_addr)

bool open(usbh_handle_t *h)
{
    USBHSH->CFG = USBHS_RST_LINK | USBHS_UH_PHY_SUSPENDM;
    USBHSH->PORT_CFG = USBHS_UH_PD_EN | USBHS_UH_HOST_EN;
    USBHSH->FRAME = USBHS_UH_SOF_CNT_EN;
    USBHSH->CFG = USBHS_UH_SOF_EN | USBHS_UD_DMA_EN | USBHS_UD_PHY_SUSPENDM;
    USBHSH->INT_EN = USBHS_UHIE_SOF_ACT | USBHS_UHIE_TRANSFER;
    USBHSH->PORT_INT_EN = USBHS_UHIE_PORT_SUSP | USBHS_UHIE_PORT_CONNECT;
    return true;
}

bool close(usbh_handle_t *h)
{
    USBHSH->CFG = USBHS_UD_RST_LINK | USBHS_UD_RST_SIE | USBHS_UD_CLR_ALL;
}

void usbhsh_handle_init(usbh_handle_t *h, uint32_t base_addr, usbhsh_ctx_t *ctx)
{
    memset(h, 0, sizeof(usbh_handle_t));
    h->base_addr = base_addr;
    h->port_ctx = ctx;

    h->open = open;
    h->close = close;
}

void usbhsh_event_handle(usbh_handle_t *h)
{
    uint8_t int_flag = USBHSH->INT_FLAG;
    USBHSH->INT_FLAG = int_flag;

    uint8_t port_change = USBHSH->PORT_STATUS_CHG;
    USBHSH->PORT_STATUS_CHG = port_change;
}
