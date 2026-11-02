/**
 * @file stm32f4xx_it.c
 * @brief Interrupt entry points: ส่งต่อ IRQ ให้ driver เท่านั้น กติกาโหลดและพลังงานอยู่ใน Application
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "drv_adc_dma.h"
#include "drv_buttons_exti.h"
#include "drv_timer.h"
#include "drv_uart_irq.h"

void DMA2_Stream0_IRQHandler(void)
{
    DrvAdcDma_IrqHandler();
}

void TIM2_IRQHandler(void)
{
    DrvTimer_IrqHandler();
}

void USART2_IRQHandler(void)
{
    DrvUart_IrqHandler();
}

void EXTI3_IRQHandler(void)
{
    DrvButtonsExti_Line3Irq();
}

void EXTI4_IRQHandler(void)
{
    DrvButtonsExti_Line4Irq();
}

void EXTI9_5_IRQHandler(void)
{
    DrvButtonsExti_Line9To5Irq();
}

void EXTI15_10_IRQHandler(void)
{
    DrvButtonsExti_Line15To10Irq();
}
