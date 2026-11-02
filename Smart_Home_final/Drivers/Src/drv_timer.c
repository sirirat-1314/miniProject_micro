/**
 * @file drv_timer.c
 * @brief TIM2: 16 MHz / (15999+1) / (99+1) = 10 Hz หรือ 100 ms ต่อ tick ตาม config เริ่มต้น
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "drv_timer.h"
#include "drv_adc_dma.h"
#include "project_config.h"
#include "stm32f411xe.h"

static volatile uint32_t s_milliseconds;
static volatile uint32_t s_pending_ticks;

/**
 * PSC ลด counter เหลือ 1 kHz; ARR กำหนด APP_TICK_MS; เปิด update interrupt
 */
void DrvTimer_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;

    TIM2->PSC = 15999UL; /* 16 MHz / 16000 = 1 kHz */
    TIM2->ARR = APP_TICK_MS - 1UL;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0UL;
    TIM2->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, 4UL);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
}

/**
 * คืนเวลา software หน่วย ms แต่ความละเอียดเป็นหนึ่ง APP_TICK_MS ไม่ใช่ 1 ms
 */
uint32_t DrvTimer_GetMs(void)
{
    return s_milliseconds;
}

/**
 * main อ่านจำนวน tick ค้างและล้างใน critical section เพื่อไม่ให้ ISR increment สูญหาย
 */
uint32_t DrvTimer_TakeTicks(void)
{
    uint32_t ticks;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ticks = s_pending_ticks;
    s_pending_ticks = 0UL;
    __set_PRIMASK(primask);
    return ticks;
}

/**
 * เพิ่มเวลา/งานค้างและเริ่ม ADC; จำกัด pending ticks 1000 เพื่อไม่สะสมไม่สิ้นสุด
 * ไม่ทำ temperature conversion หรือ UART formatting ใน ISR
 */
void DrvTimer_IrqHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) != 0UL)
    {
        TIM2->SR &= ~TIM_SR_UIF;
        s_milliseconds += APP_TICK_MS;
        if (s_pending_ticks < 1000UL)
        {
            s_pending_ticks++;
        }
        DrvAdcDma_StartFromIsr();
    }
}
