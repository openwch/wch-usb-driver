/**
 * @file usbfsd_port.c
 * @author Links (lhd@wch.cn)
 * @brief USB Full-Speed device port source file
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include "usb_driver.h"
#include "usbfs_port.h"

/* @define */
#define USBFSD           ((usbfsd_ip_t *)h->base_addr)
#define ENDP_TX_LEN(ep)  *((volatile uint16_t *)&(USBFSD->UEP0_TX_LEN) + (ep) * 2)
#define ENDP_TX_CTRL(ep) *((volatile uint8_t *)&(USBFSD->UEP0_TX_CTRL) + (ep) * 4)
#define ENDP_RX_CTRL(ep) *((volatile uint8_t *)&(USBFSD->UEP0_RX_CTRL) + (ep) * 4)

static bool open(usbd_handle_t *h, usb_speed_t speed, bool sof_en)
{
    if (!h || speed == USB_SPEED_UNKNOWN || speed > USB_SPEED_HIGH) return false;

    usbfsd_ctx_t *port_ctx = (usbfsd_ctx_t *)h->port_ctx;
    USBFSD->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    port_ctx->delay_us(10);
    USBFSD->BASE_CTRL = 0x00;
    USBFSD->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER | (sof_en ? USBFS_UIE_DEV_SOF : 0);
    USBFSD->UEP4_1_MOD = 0;
    USBFSD->UEP2_3_MOD = 0;
    USBFSD->UEP5_6_MOD = 0;
    USBFSD->UEP7_MOD = 0;
    USBFSD->UEP0_DMA = (uint32_t)port_ctx->endp_dma_bufs[0];
    USBFSD->UEP1_DMA = (uint32_t)port_ctx->endp_dma_bufs[1];
    USBFSD->UEP2_DMA = (uint32_t)port_ctx->endp_dma_bufs[2];
    USBFSD->UEP3_DMA = (uint32_t)port_ctx->endp_dma_bufs[3];
    USBFSD->UEP4_DMA = (uint32_t)port_ctx->endp_dma_bufs[4];
    USBFSD->UEP5_DMA = (uint32_t)port_ctx->endp_dma_bufs[5];
    USBFSD->UEP6_DMA = (uint32_t)port_ctx->endp_dma_bufs[6];
    USBFSD->UEP7_DMA = (uint32_t)port_ctx->endp_dma_bufs[7];
    port_ctx->dma_buf_ptrs[USB_DIR_IN][0] = port_ctx->endp_dma_bufs[0];
    port_ctx->dma_buf_ptrs[USB_DIR_OUT][0] = port_ctx->endp_dma_bufs[0];
    USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN |
                        (speed == USB_SPEED_LOW ? USBFS_UC_LOW_SPEED : 0);
    USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | (speed == USB_SPEED_LOW ? USBFS_UD_LOW_SPEED : 0) | USBFS_UD_PORT_EN;
    return true;
}

static bool close(usbd_handle_t *h)
{
    if (!h) return false;

    usbfsd_ctx_t *port_ctx = (usbfsd_ctx_t *)h->port_ctx;
    USBFSD->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    port_ctx->delay_us(10);
    USBFSD->BASE_CTRL = 0x00;
    return true;
}

static bool resume(usbd_handle_t *h)
{
    usbfsd_ctx_t *port_ctx = (usbfsd_ctx_t *)h->port_ctx;
    USBFSD->UDEV_CTRL ^= USBFS_UD_LOW_SPEED;
    port_ctx->delay_ms(8);
    USBFSD->UDEV_CTRL ^= USBFS_UD_LOW_SPEED;
    port_ctx->delay_ms(1);
    return true;
}

static bool set_address(usbd_handle_t *h, uint8_t address)
{
    USBFSD->DEV_ADDR = address;
    return true;
}

static usb_speed_t get_link_speed(usbd_handle_t *h)
{
    return USBFSD->UDEV_CTRL & USBFS_UD_LOW_SPEED ? USB_SPEED_LOW : USB_SPEED_FULL;
}

static bool test_mode_ctrl(usbd_handle_t *h, usb_test_select_t test_mode)
{
    return false;
}

static bool endp_open(usbd_handle_t *h, usb_endp_t endp, usb_endp_type_t type, uint16_t mps)
{
    if (!h || USB_ENDP_NUM(endp) >= 8) return false;

    usbfsd_ctx_t *port_ctx = (usbfsd_ctx_t *)h->port_ctx;
    uint8_t dir = USB_ENDP_DIR(endp);
    uint8_t num = USB_ENDP_NUM(endp);
    uint8_t **in_ptr = &port_ctx->dma_buf_ptrs[USB_DIR_IN][num];
    uint8_t **out_ptr = &port_ctx->dma_buf_ptrs[USB_DIR_OUT][num];

    port_ctx->xfer_ctxs[dir ? USB_DIR_IN : USB_DIR_OUT][num].mps = mps;

    if (dir)
    {
        ENDP_TX_CTRL(num) &= ~USBFS_UEP_T_AUTO_TOG;
        ENDP_TX_CTRL(num) = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
    }
    else
    {
        ENDP_RX_CTRL(num) &= ~USBFS_UEP_R_AUTO_TOG;
        ENDP_RX_CTRL(num) = USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_NAK;
    }

    switch (num)
    {
    case 1:
        USBFSD->UEP4_1_MOD |= dir ? USBFS_UEP1_TX_EN : USBFS_UEP1_RX_EN;
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP4_1_MOD & USBFS_UEP1_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 2:
        USBFSD->UEP2_3_MOD |= dir ? USBFS_UEP2_TX_EN : USBFS_UEP2_RX_EN;
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP2_3_MOD & USBFS_UEP2_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 3:
        USBFSD->UEP2_3_MOD |= dir ? USBFS_UEP3_TX_EN : USBFS_UEP3_RX_EN;
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP2_3_MOD & USBFS_UEP3_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 4:
        USBFSD->UEP4_1_MOD |= dir ? USBFS_UEP4_TX_EN : USBFS_UEP4_RX_EN;
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP4_1_MOD & USBFS_UEP4_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 5:
        USBFSD->UEP5_6_MOD |= dir ? USBFS_UEP5_TX_EN : USBFS_UEP5_RX_EN;
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP5_6_MOD & USBFS_UEP5_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 6:
        USBFSD->UEP5_6_MOD |= dir ? USBFS_UEP6_TX_EN : USBFS_UEP6_RX_EN;
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP5_6_MOD & USBFS_UEP6_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 7:
        USBFSD->UEP7_MOD |= dir ? USBFS_UEP7_TX_EN : USBFS_UEP7_RX_EN;
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP7_MOD & USBFS_UEP7_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;
    }
    return true;
}

static bool endp_close(usbd_handle_t *h, usb_endp_t endp)
{
    if (!h || USB_ENDP_NUM(endp) >= 8) return false;

    usbfsd_ctx_t *port_ctx = (usbfsd_ctx_t *)h->port_ctx;
    uint8_t dir = USB_ENDP_DIR(endp);
    uint8_t num = USB_ENDP_NUM(endp);
    uint8_t **in_ptr = &port_ctx->dma_buf_ptrs[USB_DIR_IN][num];
    uint8_t **out_ptr = &port_ctx->dma_buf_ptrs[USB_DIR_OUT][num];

    switch (num)
    {
    case 1:
        USBFSD->UEP4_1_MOD &= ~(dir ? USBFS_UEP1_TX_EN : USBFS_UEP1_RX_EN);
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP4_1_MOD & USBFS_UEP1_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 2:
        USBFSD->UEP2_3_MOD &= ~(dir ? USBFS_UEP2_TX_EN : USBFS_UEP2_RX_EN);
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP2_3_MOD & USBFS_UEP2_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 3:
        USBFSD->UEP2_3_MOD &= ~(dir ? USBFS_UEP3_TX_EN : USBFS_UEP3_RX_EN);
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP2_3_MOD & USBFS_UEP3_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 4:
        USBFSD->UEP4_1_MOD &= ~(dir ? USBFS_UEP4_TX_EN : USBFS_UEP4_RX_EN);
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP4_1_MOD & USBFS_UEP4_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 5:
        USBFSD->UEP5_6_MOD &= ~(dir ? USBFS_UEP5_TX_EN : USBFS_UEP5_RX_EN);
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP5_6_MOD & USBFS_UEP5_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 6:
        USBFSD->UEP5_6_MOD &= ~(dir ? USBFS_UEP6_TX_EN : USBFS_UEP6_RX_EN);
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP5_6_MOD & USBFS_UEP6_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;

    case 7:
        USBFSD->UEP7_MOD &= ~(dir ? USBFS_UEP7_TX_EN : USBFS_UEP7_RX_EN);
        *in_ptr = &port_ctx->endp_dma_bufs[num][USBFSD->UEP7_MOD & USBFS_UEP7_RX_EN ? 64 : 0];
        *out_ptr = &port_ctx->endp_dma_bufs[num][0];
        break;
    }
    return true;
}

static bool endp_stall(usbd_handle_t *h, usb_endp_t endp, bool stall)
{
    uint8_t num = USB_ENDP_NUM(endp);

    if (!h || num >= 8) return false;

    if (USB_ENDP_DIR(endp))
    {
        ENDP_TX_CTRL(num) &= ~USBFS_UEP_T_AUTO_TOG;
        ENDP_TX_CTRL(num) = USBFS_UEP_T_AUTO_TOG | (stall ? USBFS_UEP_T_RES_STALL : USBFS_UEP_T_RES_NAK);
    }
    else
    {
        ENDP_RX_CTRL(num) &= ~USBFS_UEP_R_AUTO_TOG;
        ENDP_RX_CTRL(num) = USBFS_UEP_R_AUTO_TOG | (stall ? USBFS_UEP_R_RES_STALL : USBFS_UEP_R_RES_NAK);
    }
    return true;
}

static bool endp_is_stalled(usbd_handle_t *h, usb_endp_t endp)
{
    uint8_t num = USB_ENDP_NUM(endp);

    if (!h || num >= 8) return false;

    if (USB_ENDP_DIR(endp))
    {
        return (ENDP_TX_CTRL(num) & USBFS_UEP_T_RES_MASK) == USBFS_UEP_T_RES_STALL;
    }
    else
    {
        return (ENDP_RX_CTRL(num) & USBFS_UEP_R_RES_MASK) == USBFS_UEP_R_RES_STALL;
    }
}

static bool endp_transfer(usbd_handle_t *h, usb_endp_t endp, void *buf, size_t len)
{
    uint8_t dir = USB_ENDP_DIR(endp);
    uint8_t num = USB_ENDP_NUM(endp);

    if (num != 0 && dir && (ENDP_TX_CTRL(num) & USBFS_UEP_T_RES_MASK) != USBFS_UEP_T_RES_NAK) return false;

    usbfsd_ctx_t *port_ctx = (usbfsd_ctx_t *)h->port_ctx;
    usbfs_xfer_ctx_t *xfer_ctx = &port_ctx->xfer_ctxs[dir ? USB_DIR_IN : USB_DIR_OUT][num];
    xfer_ctx->xfer_buf = buf;
    xfer_ctx->xfer_len = len;
    xfer_ctx->xfer_ofs = 0;

    if (dir)
    {
        size_t xfer_len = USB_MIN(len, xfer_ctx->mps);
        memcpy(port_ctx->dma_buf_ptrs[USB_DIR_IN][num], buf, xfer_len);
        ENDP_TX_LEN(num) = xfer_len;
        ENDP_TX_CTRL(num) = (ENDP_TX_CTRL(num) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
    }
    else
    {
        ENDP_RX_CTRL(num) = (ENDP_RX_CTRL(num) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
    }
    return true;
}

void usbfsd_handle_init(usbd_handle_t *h, uint32_t base_addr, usbfsd_ctx_t *ctx)
{
    memset(h, 0, sizeof(usbd_handle_t));
    h->base_addr = base_addr;
    h->port_ctx = ctx;

    h->open = open;
    h->close = close;
    h->resume = resume;
    h->set_address = set_address;
    h->get_link_speed = get_link_speed;
    h->test_mode_ctrl = test_mode_ctrl;
    h->endp_open = endp_open;
    h->endp_close = endp_close;
    h->endp_stall = endp_stall;
    h->endp_is_stalled = endp_is_stalled;
    h->endp_transfer = endp_transfer;
}

void usbfsd_event_handle(usbd_handle_t *h)
{
    usbfsd_ctx_t *port_ctx = (usbfsd_ctx_t *)h->port_ctx;
    usbd_port_event_ctx_t event_ctx;
    uint8_t flag = USBFSD->INT_FG;

    if (flag & USBFS_UIF_TRANSFER)
    {
        uint8_t stat = USBFSD->INT_ST;
        uint8_t endp = stat & USBFS_UIS_ENDP_MASK;
        uint8_t token = stat & USBFS_UIS_TOKEN_MASK;

        switch (token)
        {
        case USBFS_UIS_TOKEN_SETUP:
        {
            /* Reset Control Endpoint Toggle */
            ENDP_TX_CTRL(0) = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;
            ENDP_RX_CTRL(0) = USBFS_UEP_T_TOG | USBFS_UEP_R_RES_NAK;

            /* Copy the setup packet from the endpoint 0 buffer to the setup structure */
            memcpy(&h->setup, port_ctx->endp_dma_bufs[0], sizeof(usb_setup_t));

            event_ctx.e = USBD_PORT_EVENT_SETUP;
            usbd_event_handle(h, &event_ctx);
            break;
        }

        case USBFS_UIS_TOKEN_IN:
        {
            size_t tx_len = ENDP_TX_LEN(endp);
            usbfs_xfer_ctx_t *xfer_ctx = &port_ctx->xfer_ctxs[USB_DIR_IN][endp];

            if (endp == 0)
            {
                /* Endpoint 0 Manual Toggle */
                USBFSD->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
            }

            xfer_ctx->xfer_ofs += tx_len;
            if (xfer_ctx->xfer_ofs >= xfer_ctx->xfer_len)
            {
                ENDP_TX_CTRL(endp) = (ENDP_TX_CTRL(endp) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                event_ctx.e = USBD_PORT_EVENT_XFER;
                event_ctx.xfer.buf = xfer_ctx->xfer_buf;
                event_ctx.xfer.len = xfer_ctx->xfer_ofs;
                event_ctx.xfer.endp = 0x80 | endp;
                usbd_event_handle(h, &event_ctx);
            }
            else if (endp == 0)
            {
                size_t xfer_len = USB_MIN(xfer_ctx->xfer_len - xfer_ctx->xfer_ofs, xfer_ctx->mps);
                memcpy(port_ctx->endp_dma_bufs[0], (uint8_t *)xfer_ctx->xfer_buf + xfer_ctx->xfer_ofs, xfer_len);
                USBFSD->UEP0_TX_LEN = xfer_len;
                USBFSD->UEP0_TX_CTRL = (USBFSD->UEP0_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            }
            else
            {
                size_t xfer_len = USB_MIN(xfer_ctx->xfer_len - xfer_ctx->xfer_ofs, xfer_ctx->mps);
                memcpy(port_ctx->dma_buf_ptrs[USB_DIR_IN][endp], (uint8_t *)xfer_ctx->xfer_buf + xfer_ctx->xfer_ofs,
                       xfer_len);
                ENDP_TX_LEN(endp) = xfer_len;
                ENDP_TX_CTRL(endp) = (ENDP_TX_CTRL(endp) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            }
            break;
        }

        case USBFS_UIS_TOKEN_OUT:
        {
            // Out toggle mismatch
            if ((stat & USBFS_UIS_TOG_OK) == 0)
            {
                ENDP_RX_CTRL(endp) = (ENDP_RX_CTRL(endp) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
                USBFSD->INT_FG = USBFS_UIF_TRANSFER;
                return;
            }

            size_t rx_len = USBFSD->RX_LEN;
            usbfs_xfer_ctx_t *xfer_ctx = &port_ctx->xfer_ctxs[USB_DIR_OUT][endp];

            if (endp == 0)
            {
                /* Endpoint 0 Manual Toggle */
                USBFSD->UEP0_RX_CTRL ^= USBFS_UEP_R_TOG;
                memcpy((uint8_t *)xfer_ctx->xfer_buf + xfer_ctx->xfer_ofs, port_ctx->endp_dma_bufs[0],
                       USB_MIN(rx_len, xfer_ctx->xfer_len - xfer_ctx->xfer_ofs));
            }
            else
            {
                memcpy((uint8_t *)xfer_ctx->xfer_buf + xfer_ctx->xfer_ofs, port_ctx->dma_buf_ptrs[USB_DIR_OUT][endp],
                       USB_MIN(rx_len, xfer_ctx->xfer_len - xfer_ctx->xfer_ofs));
            }

            xfer_ctx->xfer_ofs = USB_MIN(xfer_ctx->xfer_ofs + rx_len, xfer_ctx->xfer_len);
            if (xfer_ctx->xfer_ofs >= xfer_ctx->xfer_len || rx_len < xfer_ctx->mps)
            {
                ENDP_RX_CTRL(endp) = (ENDP_RX_CTRL(endp) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_NAK;
                event_ctx.e = USBD_PORT_EVENT_XFER;
                event_ctx.xfer.buf = xfer_ctx->xfer_buf;
                event_ctx.xfer.len = xfer_ctx->xfer_ofs;
                event_ctx.xfer.endp = 0x00 | endp;
                usbd_event_handle(h, &event_ctx);
            }
            else
            {
                ENDP_RX_CTRL(endp) = (ENDP_RX_CTRL(endp) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
            }
            break;
        }

        case USBFS_UIS_TOKEN_SOF:
        {
            event_ctx.e = USBD_PORT_EVENT_SOF;
            event_ctx.sof.frame_num = 0;
            event_ctx.sof.mframe_num = 0;
            usbd_event_handle(h, &event_ctx);
            break;
        }
        }

        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    }
    else if (flag & USBFS_UIF_BUS_RST)
    {
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
        USBFSD->UEP4_1_MOD = 0;
        USBFSD->UEP2_3_MOD = 0;
        USBFSD->UEP5_6_MOD = 0;
        USBFSD->UEP7_MOD = 0;
        event_ctx.e = USBD_PORT_EVENT_RESET;
        usbd_event_handle(h, &event_ctx);
    }
    else if (flag & USBFS_UIF_SUSPEND)
    {
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
        if (USBFSD->MIS_ST & USBFS_UMS_SUSPEND)
        {
            event_ctx.e = USBD_PORT_EVENT_SUSPEND;
            usbd_event_handle(h, &event_ctx);
        }
    }
    else
    {
        USBFSD->INT_FG = flag;
    }
}
