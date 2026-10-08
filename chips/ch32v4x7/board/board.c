/**
 * @file board.c
 * @author Links (lhd@wch.cn)
 * @brief Board support package
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include "board.h"
#include "ch32v4x7.h"
#include "usb_driver.h"
#include "usbhs_port.h"

/* @define */
#define USBHS1D_BASE_ADDR 0x40024000
#define USBHS1H_BASE_ADDR 0x40024100
#define USBHS2D_BASE_ADDR 0x40023400
#define USBHS2H_BASE_ADDR 0x40023500

/* @enum */
typedef enum
{
    USBHS1_INDEX,
    USBHS2_INDEX,
} usb_index_t;

typedef enum
{
    USB_MODE_IDLE,
    USB_MODE_DEVICE,
    USB_MODE_HOST,
} usb_mode_t;

/* @global */
static usb_mode_t usb_modes[USB_COUNT];
static usbd_handle_t usbd_handles[USB_COUNT];
static usbhsd_ctx_t usbhsd_ctx[USB_COUNT];

void board_init(void)
{
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(921600);

    printf("======== Startup Information ========\r\n");
    printf("Compiled Time: %s %s\n", __DATE__, __TIME__);
    printf("RISC-V Compiler: %s\r\n", __VERSION__);
    printf("System Clock: %ld\r\n", SystemCoreClock);
    printf("=====================================\r\n\r\n");
}

usbd_handle_t *board_usbd_init(uint8_t index)
{
    if (index >= USB_COUNT || usb_modes[index] != USB_MODE_IDLE) return NULL;

    switch (index)
    {
    case USBHS1_INDEX:
        if ((RCC->CTLR & RCC_USBHSPLLRDY) == 0)
        {
            RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS1, DISABLE);
            RCC->CTLR &= ~RCC_USBHSPLLON;
            if (RCC->CTLR & RCC_HSEON)
            {
                RCC_USBHSPLLCLKConfig(RCC_USBHSPLLCLKSource_HSE);
                RCC_USBHSPLLReferConfig(RCC_USBHSPLLCKREFCLK_25M);
            }
            else
            {
                RCC_USBHSPLLCLKConfig(RCC_USBHSPLLCLKSource_HSI);
                RCC_USBHSPLLReferConfig(RCC_USBHSPLLCKREFCLK_20M);
            }
            RCC->CTLR |= RCC_USBHSPLLON;
            while (!(RCC->CTLR & RCC_USBHSPLLRDY));
        }

        /* Enable UTMI1 interface */
        RCC_UTMI1cmd(ENABLE);

        /* Enable USBHS1 clock */
        RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS1, ENABLE);

        /* Enable USBHS1 interrupt */
        NVIC_EnableIRQ(USBHS1_IRQn);

        /* Initialize USBHS1 device handle */
        usb_modes[USBHS1_INDEX] = USB_MODE_DEVICE;
        memset(&usbhsd_ctx[USBHS1_INDEX], 0, sizeof(usbhsd_ctx[USBHS1_INDEX]));
        usbhsd_handle_init(&usbd_handles[USBHS1_INDEX], USBHS1D_BASE_ADDR, &usbhsd_ctx[USBHS1_INDEX]);
        return &usbd_handles[USBHS1_INDEX];

    case USBHS2_INDEX:
        if ((RCC->CTLR & RCC_USBHSPLLRDY) == 0)
        {
            RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS2, DISABLE);
            RCC->CTLR &= ~RCC_USBHSPLLON;
            if (RCC->CTLR & RCC_HSEON)
            {
                RCC_USBHSPLLCLKConfig(RCC_USBHSPLLCLKSource_HSE);
                RCC_USBHSPLLReferConfig(RCC_USBHSPLLCKREFCLK_25M);
            }
            else
            {
                RCC_USBHSPLLCLKConfig(RCC_USBHSPLLCLKSource_HSI);
                RCC_USBHSPLLReferConfig(RCC_USBHSPLLCKREFCLK_20M);
            }
            RCC->CTLR |= RCC_USBHSPLLON;
            while (!(RCC->CTLR & RCC_USBHSPLLRDY));
        }

        RCC_UTMI2cmd(ENABLE);
        RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS2, ENABLE);
        NVIC_EnableIRQ(USBHS2_IRQn);

        usb_modes[USBHS2_INDEX] = USB_MODE_DEVICE;
        memset(&usbhsd_ctx[USBHS2_INDEX], 0, sizeof(usbhsd_ctx[USBHS2_INDEX]));
        usbhsd_handle_init(&usbd_handles[USBHS2_INDEX], USBHS2D_BASE_ADDR, &usbhsd_ctx[USBHS2_INDEX]);
        return &usbd_handles[USBHS2_INDEX];

    default:
        return NULL;
    }
}

usbd_handle_t *board_usbd_deinit(uint8_t index)
{
    if (index >= USB_COUNT) return NULL;

    switch (index)
    {
    case USBHS1_INDEX:
        /* Disable USBHS1 interrupt */
        NVIC_DisableIRQ(USBHS1_IRQn);

        /* Disable USBHS1 clock and UTMI1 interface */
        RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS1, DISABLE);
        RCC_UTMI1cmd(DISABLE);

        /* Disable USBHS PLL if both USBHS1 and USBHS2 are not enabled */
        if (!(RCC->HBPCENR & (RCC_HBPeriph_USBHS1 | RCC_HBPeriph_USBHS2)))
        {
            RCC->CTLR &= ~RCC_USBHSPLLON;
        }

        /* Mark USB device as idle */
        usb_modes[USBHS1_INDEX] = USB_MODE_IDLE;
        return &usbd_handles[USBHS1_INDEX];

    case USBHS2_INDEX:
        NVIC_DisableIRQ(USBHS2_IRQn);
        RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS2, DISABLE);
        RCC_UTMI2cmd(DISABLE);
        if (!(RCC->HBPCENR & (RCC_HBPeriph_USBHS1 | RCC_HBPeriph_USBHS2)))
        {
            RCC->CTLR &= ~RCC_USBHSPLLON;
        }
        usb_modes[USBHS2_INDEX] = USB_MODE_IDLE;
        return &usbd_handles[USBHS2_INDEX];

    default:
        return NULL;
    }
}

__attribute__((interrupt("WCH-Interrupt-fast"))) void USBHS1_IRQHandler(void)
{
    switch (usb_modes[USBHS1_INDEX])
    {
    case USB_MODE_DEVICE:
        usbhsd_event_handle(&usbd_handles[USBHS1_INDEX]);
        break;

    case USB_MODE_HOST:
        break;
    }
}

__attribute__((interrupt("WCH-Interrupt-fast"))) void USBHS2_IRQHandler(void)
{
    switch (usb_modes[USBHS2_INDEX])
    {
    case USB_MODE_DEVICE:
        usbhsd_event_handle(&usbd_handles[USBHS2_INDEX]);
        break;

    case USB_MODE_HOST:
        break;
    }
}
