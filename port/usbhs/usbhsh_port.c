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
#define USBHSH ((usbhsh_ip_t *)((usbhsh_ctx_t *)h->port_ctx)->base_addr)

static bool open(usbh_handle_t *h)
{
    USBHSH->CFG = USBHS_RST_LINK | USBHS_UH_PHY_SUSPENDM;
    USBHSH->PORT_CFG = USBHS_UH_PD_EN | USBHS_UH_HOST_EN;
    USBHSH->FRAME = USBHS_UH_SOF_CNT_EN;
    USBHSH->CFG = USBHS_UH_SOF_EN | USBHS_UD_DMA_EN | USBHS_UD_PHY_SUSPENDM;
    USBHSH->INT_EN = USBHS_UHIE_SOF_ACT | USBHS_UHIE_TRANSFER;
    USBHSH->PORT_INT_EN = USBHS_UHIE_PORT_SUSP | USBHS_UHIE_PORT_CONNECT;
    return true;
}

static bool close(usbh_handle_t *h)
{
    USBHSH->CFG = USBHS_UD_RST_LINK | USBHS_UD_RST_SIE | USBHS_UD_CLR_ALL;
    return true;
}

static void start_transfer(usbh_handle_t *h)
{
}

static void root_set_feature(usbh_handle_t *h, usbh_port_feature_t feature)
{
    switch (feature)
    {
    case USBH_PORT_FEATURE_SUSPEND:
        USBHSH->PORT_CTRL |= USBHS_UH_SET_PORT_SUSP;
        break;

    case USBH_PORT_FEATURE_RESET:
        USBHSH->PORT_CTRL |= USBHS_UH_SET_PORT_RESET;
        break;
    }
}

static void root_get_status(usbh_handle_t *h, usbh_port_status_t *status)
{
    status->status = 0;

    uint16_t port_status = USBHSH->PORT_STATUS;
    status->status_bits.power = 1;
    if (port_status & USBHS_UHIS_PORT_CONNECT) status->status_bits.connect = 1;
    if (port_status & USBHS_UHIS_PORT_EN) status->status_bits.enable = 1;
    if (port_status & USBHS_UHIS_PORT_SUSP) status->status_bits.suspend = 1;
    if (port_status & USBHS_UHIS_PORT_RST) status->status_bits.reset = 1;
    if (port_status & USBHS_UHIS_PORT_LS) status->status_bits.low_speed = 1;
    if (port_status & USBHS_UHIS_PORT_HS) status->status_bits.high_speed = 1;
    if (port_status & USBHS_UHIS_PORT_TEST) status->status_bits.test_mode = 1;
}

void usbhsh_handle_init(usbh_handle_t *h, usbhsh_ctx_t *ctx)
{
    memset(h, 0, sizeof(usbh_handle_t));
    h->port_ctx = ctx;

    h->open = open;
    h->close = close;
    h->start_transfer = start_transfer;
    h->root_set_feature = root_set_feature;
    h->root_get_status = root_get_status;
}

void usbhsh_event_handle(usbh_handle_t *h)
{
    /* Handle USB host controller interrupts */
    uint8_t int_flag = USBHSH->INT_FLAG;
    if (int_flag & USBHS_UHIF_TRANSFER)
    {
        USBHSH->INT_FLAG = USBHS_UHIF_TRANSFER;
    }
    else if (int_flag & USBHS_UHIF_SOF_ACT)
    {
        USBHSH->INT_FLAG = USBHS_UHIF_SOF_ACT;
    }
    else
    {
        USBHSH->INT_FLAG = int_flag;
    }

    /* Handle USB host port status change interrupts */
    uint8_t port_change = USBHSH->PORT_STATUS_CHG;
    if (port_change & USBHS_UHIF_PORT_CONNECT)
    {
        h->root_port_change = true;
        USBHSH->PORT_STATUS_CHG = USBHS_UHIF_PORT_CONNECT;
    }
    else if (port_change & USBHS_UHIF_PORT_SUSP)
    {
        USBHSH->PORT_STATUS_CHG = USBHS_UHIF_PORT_SUSP;
    }
    else
    {
        USBHSH->PORT_STATUS_CHG = port_change;
    }
}
