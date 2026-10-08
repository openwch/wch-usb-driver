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

typedef enum
{
    USB_MODE_IDLE,
    USB_MODE_DEVICE,
    USB_MODE_HOST,
} usb_mode_t;

/* @global */
static usb_mode_t usb_modes[USB_COUNT];
static usbd_handle_t usbd_handles[USB_COUNT];
static usbfsd_ctx_t usbfsd_ctx;

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

usbd_handle_t *board_usbd_init(uint8_t index)
{
    if (index >= USB_COUNT || usb_modes[index] != USB_MODE_IDLE) return NULL;

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

        /* Initialize USBFS device handle */
        usb_modes[USBFS_INDEX] = USB_MODE_DEVICE;
        memset(&usbfsd_ctx, 0, sizeof(usbfsd_ctx));
        usbfsd_ctx.delay_us = Delay_Us;
        usbfsd_ctx.delay_ms = Delay_Ms;
        usbfsd_handle_init(&usbd_handles[USBFS_INDEX], USBFSD_BASE_ADDR, &usbfsd_ctx);
        return &usbd_handles[USBFS_INDEX];

    default:
        return NULL;
    }
}

usbd_handle_t *board_usbd_deinit(uint8_t index)
{
    if (index >= USB_COUNT) return NULL;

    switch (index)
    {
    case USBFS_INDEX:
        /* Disable USBFS interrupt */
        NVIC_DisableIRQ(USBFS_IRQn);

        /* Disable USBFS clock */
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, DISABLE);

        /* Mark USB device as idle */
        usb_modes[USBFS_INDEX] = USB_MODE_IDLE;
        return &usbd_handles[USBFS_INDEX];

    default:
        return NULL;
    }
}

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
