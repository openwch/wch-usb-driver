/**
 * @file board.c
 * @author Links (lhd@wch.cn)
 * @brief Board support package
 *
 * @copyright Copyright (c) 2026
 *
 */

/* @include */
#include "ch32v205.h"
#include "usb_driver.h"
#include "usbfs_port.h"
#include "usbhs_port.h"

/* @define */
#define USBHSD_BASE_ADDR 0x40023400
#define USBHSH_BASE_ADDR 0x40023500
#define USBFSD_BASE_ADDR 0x50000000
#define USBFSH_BASE_ADDR 0x50000000

/* @enum */
typedef enum
{
    USBFS_INDEX,
    USBHS_INDEX,
    USB_COUNT,
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

#ifdef USBFS
static usbfsd_ctx_t usbfsd_ctx;
#endif
#ifdef USBHS
static usbhsd_ctx_t usbhsd_ctx;
#endif

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
    if (index >= USB_ARRAY_SIZE(usb_modes) || usb_modes[index] != USB_MODE_IDLE) return NULL;

    switch (index)
    {
#ifdef USBFS
    case USBFS_INDEX:
        /* Initialize USBHS PLL if not already enabled */
        if (!(RCC->CTLR & RCC_USBHS_PLLRDY))
        {
            RCC_USBHS_PLLCmd(DISABLE);
            RCC_USBHSPLLCLKConfig((RCC->CTLR & RCC_HSERDY) ? RCC_USBHSPLLSource_HSE : RCC_USBHSPLLSource_HSI);
            RCC_USBHSPLLReferConfig(RCC_USBHSPLLRefer_8M);
            RCC_USBHSPLLClockSourceDivConfig(RCC_USBHSPLL_IN_Div1);
            RCC_USBHS_PLLCmd(ENABLE);
            while (!(RCC->CTLR & RCC_USBHS_PLLRDY));
        }

        /* Configure USBFS clock source */
        RCC_USBFSCLKConfig(RCC_USBFSCLKSource_USBHSPLL);

        /* Enable USBFS Clock */
        RCC_HBPeriphClockCmd(RCC_HBPeriph_USBFS, ENABLE);

        /* Enable USBFS interrupt */
        NVIC_EnableIRQ(USBFS_IRQn);

        /* Initialize USBFS device handle */
        usb_modes[USBFS_INDEX] = USB_MODE_DEVICE;
        memset(&usbfsd_ctx, 0, sizeof(usbfsd_ctx));
        usbfsd_ctx.delay_us = Delay_Us;
        usbfsd_ctx.delay_ms = Delay_Ms;
        usbfsd_handle_init(&usbd_handles[USBFS_INDEX], USBFSD_BASE_ADDR, &usbfsd_ctx);
        return &usbd_handles[USBFS_INDEX];
#endif

#ifdef USBHS
    case USBHS_INDEX:
        if (!(RCC->CTLR & RCC_USBHS_PLLRDY))
        {
            RCC_USBHS_PLLCmd(DISABLE);
            RCC_USBHSPLLCLKConfig((RCC->CTLR & RCC_HSERDY) ? RCC_USBHSPLLSource_HSE : RCC_USBHSPLLSource_HSI);
            RCC_USBHSPLLReferConfig(RCC_USBHSPLLRefer_8M);
            RCC_USBHSPLLClockSourceDivConfig(RCC_USBHSPLL_IN_Div1);
            RCC_USBHS_PLLCmd(ENABLE);
            while (!(RCC->CTLR & RCC_USBHS_PLLRDY));
        }
        RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS, ENABLE);
        NVIC_EnableIRQ(USBHS_IRQn);
        usb_modes[USBHS_INDEX] = USB_MODE_DEVICE;
        memset(&usbhsd_ctx, 0, sizeof(usbhsd_ctx));
        usbhsd_handle_init(&usbd_handles[USBHS_INDEX], USBHSD_BASE_ADDR, &usbhsd_ctx);
        return &usbd_handles[USBHS_INDEX];
#endif

    default:
        return NULL;
    }
}

usbd_handle_t *board_usbd_deinit(uint8_t index)
{
    switch (index)
    {
#ifdef USBFS
    case USBFS_INDEX:
        /* Disable USBFS interrupt */
        NVIC_DisableIRQ(USBFS_IRQn);

        /* Disable USBFS clock */
        RCC_HBPeriphClockCmd(RCC_HBPeriph_USBFS, DISABLE);

        /* Disable USBHS PLL if both USBFS and USBHS are not enabled */
        if (!(RCC->HBPCENR & (RCC_HBPeriph_USBFS | RCC_HBPeriph_USBHS)))
        {
            RCC_USBHS_PLLCmd(DISABLE);
        }

        /* Mark USB device as idle */
        usb_modes[USBFS_INDEX] = USB_MODE_IDLE;
        return &usbd_handles[USBFS_INDEX];
#endif

#ifdef USBHS
    case USBHS_INDEX:
        NVIC_DisableIRQ(USBHS_IRQn);
        RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS, DISABLE);
        if (!(RCC->HBPCENR & (RCC_HBPeriph_USBFS | RCC_HBPeriph_USBHS)))
        {
            RCC_USBHS_PLLCmd(DISABLE);
        }
        usb_modes[USBHS_INDEX] = USB_MODE_IDLE;
        return &usbd_handles[USBHS_INDEX];
#endif

    default:
        return NULL;
    }
}

#ifdef USBFS
__attribute__((interrupt("WCH-Interrupt-fast"))) void USBFS_IRQHandler(void)
{
    switch (usb_modes[USBFS_INDEX])
    {
    case USB_MODE_DEVICE:
        usbfsd_event_handle(&usbd_handles[USBFS_INDEX]);
        break;

    case USB_MODE_HOST:
        break;
    }
}
#endif

#ifdef USBHS
__attribute__((interrupt("WCH-Interrupt-fast"))) void USBHS_IRQHandler(void)
{
    switch (usb_modes[USBHS_INDEX])
    {
    case USB_MODE_DEVICE:
        usbhsd_event_handle(&usbd_handles[USBHS_INDEX]);
        break;

    case USB_MODE_HOST:
        break;
    }
}
#endif
