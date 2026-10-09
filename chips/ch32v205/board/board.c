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
#include "ch32v205.h"
#include "usb_driver.h"
#include "usbfs_port.h"
#include "usbhs_port.h"

/* @define */
#define USBFSD_BASE_ADDR 0x50000000
#define USBFSH_BASE_ADDR 0x50000000
#define USBHSD_BASE_ADDR 0x40023400
#define USBHSH_BASE_ADDR 0x40023500

/* @enum */
typedef enum
{
    USBFS_INDEX,
    USBHS_INDEX,
} usb_index_t;

/* @global */
static usb_mode_t usb_modes[USB_COUNT];

#ifdef USB_DEVICE_DRIVER_EN
static usbd_handle_t usbd_handles[USB_COUNT];
static usbfsd_ctx_t usbfsd_ctx;
static usbhsd_ctx_t usbhsd_ctx;
#endif

#ifdef USB_HOST_DRIVER_EN
static usbh_handle_t usbh_handles[USB_COUNT];
// static usbfsh_ctx_t usbfsh_ctx;
// static usbhsh_ctx_t usbhsh_ctx;
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

void *board_usb_init(uint8_t index, usb_mode_t mode)
{
    if (index >= USB_COUNT || mode == USB_MODE_IDLE || usb_modes[index] != USB_MODE_IDLE) return NULL;

    switch (index)
    {
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
        break;

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
        break;

    default:
        return NULL;
    }

    switch (mode)
    {
#ifdef USB_DEVICE_DRIVER_EN
    case USB_MODE_DEVICE:
    {
        static const uint32_t usbd_base_addrs[] = {USBFSD_BASE_ADDR, USBHSD_BASE_ADDR};
        usb_modes[index] = USB_MODE_DEVICE;
        if (index == USBFS_INDEX)
        {
            memset(&usbfsd_ctx, 0, sizeof(usbfsd_ctx_t));
            usbfsd_ctx.delay_us = Delay_Us;
            usbfsd_ctx.delay_ms = Delay_Ms;
            usbfsd_handle_init(&usbd_handles[index], usbd_base_addrs[index], &usbfsd_ctx);
        }
        else
        {
            memset(&usbhsd_ctx, 0, sizeof(usbhsd_ctx_t));
            usbhsd_handle_init(&usbd_handles[index], usbd_base_addrs[index], &usbhsd_ctx);
        }

        return &usbd_handles[index];
    }
#endif

#ifdef USB_HOST_DRIVER_EN
    case USB_MODE_HOST:
    {
        // static const uint32_t usbh_base_addrs[] = {USBFSH_BASE_ADDR, USBHSH_BASE_ADDR};
        usb_modes[index] = USB_MODE_HOST;
        // if (index == USBFS_INDEX)
        // {
        //     memset(&usbfsh_ctx, 0, sizeof(usbfsh_ctx_t));
        //     usbfsh_handle_init(&usbh_handles[index], usbh_base_addrs[index], &usbfsh_ctx);
        // }
        // else
        // {
        //     memset(&usbhsh_ctx, 0, sizeof(usbhsh_ctx_t));
        //     usbhsh_handle_init(&usbh_handles[index], usbh_base_addrs[index], &usbhsh_ctx);
        // }
        return &usbh_handles[index];
    }
#endif
    }

    return NULL;
}

void board_usb_deinit(uint8_t index)
{
    if (index >= USB_COUNT) return;

    switch (index)
    {
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
        break;

    case USBHS_INDEX:
        NVIC_DisableIRQ(USBHS_IRQn);
        RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS, DISABLE);
        if (!(RCC->HBPCENR & (RCC_HBPeriph_USBFS | RCC_HBPeriph_USBHS)))
        {
            RCC_USBHS_PLLCmd(DISABLE);
        }
        break;
    }

    usb_modes[index] = USB_MODE_IDLE;
}

__attribute__((interrupt("WCH-Interrupt-fast"))) void USBFS_IRQHandler(void)
{
    switch (usb_modes[USBFS_INDEX])
    {
#ifdef USB_DEVICE_DRIVER_EN
    case USB_MODE_DEVICE:
        usbfsd_event_handle(&usbd_handles[USBFS_INDEX]);
        break;
#endif

#ifdef USB_HOST_DRIVER_EN
    case USB_MODE_HOST:
        break;
#endif
    }
}

__attribute__((interrupt("WCH-Interrupt-fast"))) void USBHS_IRQHandler(void)
{
    switch (usb_modes[USBHS_INDEX])
    {
#ifdef USB_DEVICE_DRIVER_EN
    case USB_MODE_DEVICE:
        usbhsd_event_handle(&usbd_handles[USBHS_INDEX]);
        break;
#endif

#ifdef USB_HOST_DRIVER_EN
    case USB_MODE_HOST:
        break;
#endif
    }
}
