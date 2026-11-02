/**
 * @file drv_clock.c
 * @brief Clock: HSI 16 MHz ไม่หาร AHB/APB; UART และ TIM2 ต้องใช้ความถี่สอดคล้องกัน
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "drv_clock.h"
#include "stm32f411xe.h"

/**
 * เลือก HSI 16 MHz จริงใน register; การแก้ CPU_CLOCK_HZ อย่างเดียวไม่เปลี่ยน clock ฮาร์ดแวร์
 * หมายเหตุ: wait ตอน init ยังไม่มี timeout ไม่ใช่ ADC/UART data polling
 */
void DrvClock_Init(void)
{
    /* ใช้ HSI 16 MHz เพื่อให้ค่าคำนวณ baud/timer คงที่และตรวจสอบง่าย */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0UL)
    {
        /* การรอ oscillator พร้อมไม่ใช่ UART/ADC polling */
    }

    RCC->CFGR &= ~(RCC_CFGR_SW | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
    {
        /* รอ hardware ยืนยันแหล่ง clock */
    }
}
