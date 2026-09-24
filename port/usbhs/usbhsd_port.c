/**
 * @file usbhsd_port.c
 * @author Links (lhd@wch.cn)
 * @brief USB High-Speed device port source file
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include "usb_driver.h"
#include "usbhs_port.h"

/* @define */
#define USBHSD               ((usbhsd_ip_t *)h->base_addr)
#define ENDP_MAX_LEN(ep)     *((volatile uint32_t *)&(USBHSD->UEP0_MAX_LEN) + (ep))
#define ENDP_TX_LEN(ep)      *((volatile uint16_t *)&(USBHSD->UEP0_TX_LEN) + (ep) * 2)
#define ENDP_RX_LEN(ep)      *((volatile uint16_t *)&(USBHSD->UEP0_RX_LEN) + (ep) * 2)
#define ENDP_RX_SIZE(ep)     *((volatile uint16_t *)&(USBHSD->UEP1_RX_SIZE) + (ep - 1) * 2)
#define ENDP_TX_CTRL(ep)     *((volatile uint8_t *)&(USBHSD->UEP0_TX_CTRL) + (ep) * 4)
#define ENDP_RX_CTRL(ep)     *((volatile uint8_t *)&(USBHSD->UEP0_RX_CTRL) + (ep) * 4)
#define ENDP_TX_DMA_ADDR(ep) *((volatile uint32_t *)&(USBHSD->UEP1_TX_DMA) + (ep - 1))
#define ENDP_RX_DMA_ADDR(ep) *((volatile uint32_t *)&(USBHSD->UEP1_RX_DMA) + (ep - 1))

/* @function declaration */
static bool open(usbd_handle_t *h, usb_speed_t speed, bool sof_en);
static bool close(usbd_handle_t *h);
static bool resume(usbd_handle_t *h);
static bool set_address(usbd_handle_t *h, uint8_t address);
static bool test_mode_ctrl(usbd_handle_t *h, usb_test_mode_t test_mode);
static bool endp_open(usbd_handle_t *h, usb_endp_t endp, usb_endp_type_t type, uint16_t mps);
static bool endp_close(usbd_handle_t *h, usb_endp_t endp);
static bool endp_stall(usbd_handle_t *h, usb_endp_t endp, bool stall);
static bool endp_is_stalled(usbd_handle_t *h, usb_endp_t endp);
static bool write(usbd_handle_t *h, usb_endp_t endp, const void *data, size_t len);
static bool read(usbd_handle_t *h, usb_endp_t endp, void *data, size_t len);

void usbhsd_handle_init(usbd_handle_t *h, uint32_t base_addr)
{
    memset(h, 0, sizeof(usbd_handle_t));
    h->base_addr = base_addr;

    h->open = open;
    h->close = close;
    h->resume = resume;
    h->set_address = set_address;
    h->test_mode_ctrl = test_mode_ctrl;
    h->endp_open = endp_open;
    h->endp_close = endp_close;
    h->endp_stall = endp_stall;
    h->endp_is_stalled = endp_is_stalled;
    h->write = write;
    h->read = read;
}

void usbhsd_event_handle(usbd_handle_t *h)
{
    usbd_event_ctx_t ctx;
    uint8_t flag = USBHSD->INT_FG;

    if (flag & USBHS_UDIF_TRANSFER)
    {
        uint8_t stat = USBHSD->INT_ST;
        uint8_t endp_dir = stat & USBHS_UDIS_EP_DIR;
        uint8_t endp_num = stat & USBHS_UDIS_EP_ID_MASK;

        /* Setup packet transfer complete */
        if (endp_num == 0 && !endp_dir && (ENDP_RX_CTRL(0) & USBHS_UEP_R_SETUP_IS))
        {
            ENDP_RX_CTRL(0) &= ~USBHS_UEP_R_DONE;
            ENDP_TX_CTRL(0) = (ENDP_TX_CTRL(0) & ~USBHS_UEP_T_TOG_MASK) | USBHS_UEP_T_TOG_DATA1;
            ENDP_RX_CTRL(0) = (ENDP_RX_CTRL(0) & ~USBHS_UEP_R_TOG_MASK) | USBHS_UEP_R_TOG_DATA1;
            ctx.e = USBD_EVENT_SETUP;
            ctx.setup.setup = &h->setup;
            usbd_event_handle(h, &ctx);
        }
        // In packet transfer complete
        else if (endp_dir)
        {
        }
        // Out packet transfer complete
        else if (ENDP_RX_CTRL(endp_num) & USBHS_UEP_R_TOG_MATCH)
        {
        }
        // Out toggle mismatch
        else
        {
        }
    }
    else if (flag & USBHS_UDIF_RX_SOF)
    {
        USBHSD->INT_FG = USBHS_UDIF_RX_SOF;
        uint16_t frame_no = USBHSD->FRAME_NO;
        ctx.e = USBD_EVENT_SOF;
        ctx.sof.frame_num = frame_no & 0x07FF;
        ctx.sof.mframe_num = frame_no >> 13;
        usbd_event_handle(h, &ctx);
    }
    else if (flag & USBHS_UDIF_BUS_RST)
    {
        USBHSD->INT_FG = USBHS_UDIF_BUS_RST;
        USBHSD->UEP0_DMA = (uint32_t)&h->setup;
        ctx.e = USBD_EVENT_RESET;
        usbd_event_handle(h, &ctx);
    }
    else if (flag & USBHS_UDIF_SUSPEND)
    {
        USBHSD->INT_FG = USBHS_UDIF_SUSPEND;
        if (USBHSD->MIS_ST & USBHS_UDMS_SUSPEND)
        {
            ctx.e = USBD_EVENT_SUSPEND;
            usbd_event_handle(h, &ctx);
        }
    }
    else
    {
        USBHSD->INT_FG = flag;
    }
}

static bool open(usbd_handle_t *h, usb_speed_t speed, bool sof_en)
{
    if (!h || speed == USB_SPEED_UNKNOWN || speed > USB_SPEED_HIGH) return false;

    uint8_t speed_reg_val[] = {USBHS_UD_SPEED_FULL, USBHS_UD_SPEED_LOW, USBHS_UD_SPEED_HIGH};
    USBHSD->CONTROL = USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM;
    USBHSD->INT_EN = USBHS_UDIE_BUS_RST | USBHS_UDIE_SUSPEND | USBHS_UDIE_TRANSFER | (sof_en ? USBHS_UDIE_SOF_ACT : 0);
    USBHSD->UEP_TX_EN = 0;
    USBHSD->UEP_RX_EN = 0;
    USBHSD->BASE_MODE = speed_reg_val[speed - 1];
    USBHSD->CONTROL = USBHS_UD_DEV_EN | USBHS_UD_DMA_EN | USBHS_UD_LPM_EN | USBHS_UD_PHY_SUSPENDM;
    return true;
}

static bool close(usbd_handle_t *h)
{
    if (!h) return false;

    USBHSD->CONTROL = USBHS_UD_RST_SIE | USBHS_UD_RST_LINK;
    return true;
}

static bool resume(usbd_handle_t *h)
{
    USBHSD->WAKE_CTRL |= USBHS_UD_REMOTE_WKUP;
    return true;
}

static bool set_address(usbd_handle_t *h, uint8_t address)
{
    USBHSD->DEV_AD = address;
    return true;
}

static bool test_mode_ctrl(usbd_handle_t *h, usb_test_mode_t test_mode)
{
    return false;
}

static bool endp_open(usbd_handle_t *h, usb_endp_t endp, usb_endp_type_t type, uint16_t mps)
{
    if (!h || USB_ENDP_NUM(endp) >= 8)
    {
        return false;
    }

    uint8_t dir = USB_ENDP_DIR(endp);
    uint8_t num = USB_ENDP_NUM(endp);
    uint32_t bit = 1 << num;

    usbd_endp_ctx_t *ctx = &h->endp_ctxs[dir ? 1 : 0][num];
    ctx->mps = mps;
    ctx->xfer_len = 0;
    ctx->xfer_ofs = 0;

    if (dir)
    {
        switch (type)
        {
        case USB_ENDP_TYPE_CTRL:
        case USB_ENDP_TYPE_INTR:
            ENDP_MAX_LEN(num) = 1024;
            USBHSD->UEP_TX_EN |= bit;
            USBHSD->UEP_TX_ISO &= ~bit;
            USBHSD->UEP_TX_TOG_AUTO |= bit;
            USBHSD->UEP_TX_BURST &= ~bit;
            USBHSD->UEP_TX_BURST_MODE &= ~bit;
            break;

        case USB_ENDP_TYPE_ISOC:
            ENDP_MAX_LEN(num) = 1024;
            USBHSD->UEP_TX_EN |= bit;
            USBHSD->UEP_TX_ISO |= bit;
            USBHSD->UEP_TX_TOG_AUTO &= ~bit;
            USBHSD->UEP_TX_BURST &= ~bit;
            USBHSD->UEP_TX_BURST_MODE &= ~bit;
            break;

        case USB_ENDP_TYPE_BULK:
            ENDP_MAX_LEN(num) = mps;
            USBHSD->UEP_TX_EN |= bit;
            USBHSD->UEP_TX_ISO &= ~bit;
            USBHSD->UEP_TX_TOG_AUTO |= bit;
            USBHSD->UEP_TX_BURST |= bit;
            USBHSD->UEP_TX_BURST_MODE |= bit;
            break;
        }
    }
    else
    {
        switch (type)
        {
        case USB_ENDP_TYPE_CTRL:
        case USB_ENDP_TYPE_INTR:
            ENDP_MAX_LEN(num) = 1024;
            USBHSD->UEP_RX_EN |= bit;
            USBHSD->UEP_RX_ISO &= ~bit;
            USBHSD->UEP_RX_TOG_AUTO |= bit;
            USBHSD->UEP_RX_BURST &= ~bit;
            USBHSD->UEP_RX_RES_MODE &= ~bit;
            break;

        case USB_ENDP_TYPE_ISOC:
            ENDP_MAX_LEN(num) = 1024;
            USBHSD->UEP_RX_EN |= bit;
            USBHSD->UEP_RX_ISO |= bit;
            USBHSD->UEP_RX_TOG_AUTO &= ~bit;
            USBHSD->UEP_RX_BURST &= ~bit;
            USBHSD->UEP_RX_RES_MODE &= ~bit;
            break;

        case USB_ENDP_TYPE_BULK:
            ENDP_MAX_LEN(num) = mps;
            USBHSD->UEP_RX_EN |= bit;
            USBHSD->UEP_RX_ISO &= ~bit;
            USBHSD->UEP_RX_TOG_AUTO |= bit;
            USBHSD->UEP_RX_BURST |= bit;
            USBHSD->UEP_RX_RES_MODE |= bit;
            break;
        }
    }

    return true;
}

static bool endp_close(usbd_handle_t *h, usb_endp_t endp)
{
    if (!h || USB_ENDP_NUM(endp) >= 8)
    {
        return false;
    }

    uint8_t dir = USB_ENDP_DIR(endp);
    uint8_t num = USB_ENDP_NUM(endp);
    uint32_t bit = 1 << num;

    usbd_endp_ctx_t *ctx = &h->endp_ctxs[dir ? 1 : 0][num];
    ctx->mps = 0;
    ctx->xfer_len = 0;
    ctx->xfer_ofs = 0;

    if (dir)
    {
        USBHSD->UEP_TX_EN &= ~bit;
        USBHSD->UEP_TX_ISO &= ~bit;
        USBHSD->UEP_TX_TOG_AUTO &= ~bit;
        USBHSD->UEP_TX_BURST &= ~bit;
        USBHSD->UEP_TX_BURST_MODE &= ~bit;
    }
    else
    {
        USBHSD->UEP_RX_EN &= ~bit;
        USBHSD->UEP_RX_ISO &= ~bit;
        USBHSD->UEP_RX_TOG_AUTO &= ~bit;
        USBHSD->UEP_RX_BURST &= ~bit;
        USBHSD->UEP_RX_RES_MODE &= ~bit;
    }

    return true;
}

static bool endp_stall(usbd_handle_t *h, usb_endp_t endp, bool stall)
{
    uint8_t num = USB_ENDP_NUM(endp);

    if (!h || num >= 8)
    {
        return false;
    }

    uint32_t bit = 1 << num;

    if (USB_ENDP_DIR(endp))
    {
        USBHSD->UEP_TX_TOG_AUTO &= ~bit;
        ENDP_TX_CTRL(num) = stall ? USBHS_UEP_T_RES_STALL : USBHS_UEP_T_RES_NAK;
        USBHSD->UEP_TX_TOG_AUTO |= bit;
    }
    else
    {
        USBHSD->UEP_RX_TOG_AUTO &= ~bit;
        ENDP_RX_CTRL(num) = stall ? USBHS_UEP_R_RES_STALL : USBHS_UEP_R_RES_NAK;
        USBHSD->UEP_RX_TOG_AUTO |= bit;
    }

    return true;
}

static bool endp_is_stalled(usbd_handle_t *h, usb_endp_t endp)
{
    uint8_t num = USB_ENDP_NUM(endp);

    if (!h || num >= 8)
    {
        return false;
    }

    uint32_t bit = 1 << num;

    if (USB_ENDP_DIR(endp))
    {
        return (ENDP_TX_CTRL(num) & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL;
    }
    else
    {
        return (ENDP_RX_CTRL(num) & USBHS_UEP_R_RES_MASK) == USBHS_UEP_R_RES_STALL;
    }
}

static bool write(usbd_handle_t *h, usb_endp_t endp, const void *data, size_t len)
{
}

static bool read(usbd_handle_t *h, usb_endp_t endp, void *data, size_t len)
{
}
