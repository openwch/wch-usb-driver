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
#include "ch32v30x.h"
#include "usb_driver.h"
#include "usbfs_port.h"

/* @define */
#define USBFSD_BASE_ADDR 0x50000000
#define USBFSH_BASE_ADDR 0x50000000

/* @enum */
typedef enum
{
    USBFS_INDEX,
} usb_index_t;

/* @global */
static usb_mode_t usb_modes[USB_COUNT];

#ifdef USB_DEVICE_DRIVER_EN
static usbd_handle_t usbd_handles[USB_COUNT];
static usbfsd_ctx_t usbfsd_ctx;
#endif

#ifdef USB_HOST_DRIVER_EN
static usbh_handle_t usbh_handles[USB_COUNT];
// static usbfsh_ctx_t usbfsh_ctx;
#endif

void board_init(void)
{
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
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
#ifdef CH32V30x_D8C
        RCC_USBCLK48MConfig(RCC_USBCLK48MCLKSource_USBPHY);
        RCC_USBHSPLLCLKConfig(RCC_HSBHSPLLCLKSource_HSE);
        RCC_USBHSConfig(RCC_USBPLL_Div2);
        RCC_USBHSPLLCKREFCLKConfig(RCC_USBHSPLLCKREFCLK_4M);
        RCC_USBHSPHYPLLALIVEcmd(ENABLE);
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBHS, ENABLE);
#else
        if (SystemCoreClock == 144000000)
        {
            RCC_USBFSCLKConfig(RCC_USBFSCLKSource_PLLCLK_Div3);
        }
        else if (SystemCoreClock == 96000000)
        {
            RCC_USBFSCLKConfig(RCC_USBFSCLKSource_PLLCLK_Div2);
        }
        else if (SystemCoreClock == 48000000)
        {
            RCC_USBFSCLKConfig(RCC_USBFSCLKSource_PLLCLK_Div1);
        }
#endif
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, ENABLE);

        /* Enable USBFS interrupt */
        NVIC_EnableIRQ(USBFS_IRQn);
        break;

    default:
        return NULL;
    }

    switch (mode)
    {
#ifdef USB_DEVICE_DRIVER_EN
    case USB_MODE_DEVICE:
    {
        static const uint32_t usbd_base_addrs[] = {USBFSD_BASE_ADDR};
        usb_modes[index] = USB_MODE_DEVICE;
        memset(&usbfsd_ctx, 0, sizeof(usbfsd_ctx_t));
        usbfsd_ctx.delay_us = Delay_Us;
        usbfsd_ctx.delay_ms = Delay_Ms;
        usbfsd_handle_init(&usbd_handles[index], usbd_base_addrs[index], &usbfsd_ctx);
        return &usbd_handles[index];
    }
#endif

#ifdef USB_HOST_DRIVER_EN
    case USB_MODE_HOST:
    {
        // static const uint32_t usbh_base_addrs[] = {USBFSH_BASE_ADDR};
        usb_modes[index] = USB_MODE_HOST;
        // memset(&usbfsh_ctx, 0, sizeof(usbfsh_ctx_t));
        // usbfsh_handle_init(&usbh_handles[index], usbh_base_addrs[index], &usbfsh_ctx);
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
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, DISABLE);
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
