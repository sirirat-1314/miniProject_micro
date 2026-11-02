/**
 * @file drv_buttons_exti.c
 * @brief ปุ่ม falling-edge EXTI: ISR เก็บ event bits; main อ่านและล้าง event ก่อนใช้กติกาควบคุม
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "drv_buttons_exti.h"
#include "drv_timer.h"
#include "project_config.h"
#include "stm32f411xe.h"

static volatile uint32_t s_events;
static volatile uint32_t s_last_ms[4];

/**
 * ยอมรับการกดเมื่อเวลา unsigned ต่างกัน >= BUTTON_DEBOUNCE_MS
 * timebase เพิ่มทีละ APP_TICK_MS จึงไม่ใช่ debounce ที่ละเอียด 1 ms
 */
static void AcceptButton(uint32_t index, uint32_t mask)
{
    const uint32_t now = DrvTimer_GetMs();
    if ((now - s_last_ms[index]) >= BUTTON_DEBOUNCE_MS)
    {
        s_last_ms[index] = now;
        s_events |= mask;
    }
}

/**
 * เลือก port ของ EXTI3/4/5/10 แล้วเปิด falling edge และ NVIC; ปุ่มกด active-low
 */
void DrvButtonsExti_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;

    /* EXTI3/4/5 มาจาก Port B; EXTI10 มาจาก Port A */
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~(15UL << 12)) | (1UL << 12);
    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[1] & ~((15UL << 0) | (15UL << 4))) |
                        (1UL << 0) | (1UL << 4);
    SYSCFG->EXTICR[2] &= ~(15UL << 8);

    EXTI->IMR |= (1UL << 3) | (1UL << 4) | (1UL << 5) | (1UL << 10);
    EXTI->RTSR &= ~((1UL << 3) | (1UL << 4) | (1UL << 5) | (1UL << 10));
    EXTI->FTSR |= (1UL << 3) | (1UL << 4) | (1UL << 5) | (1UL << 10);
    EXTI->PR = (1UL << 3) | (1UL << 4) | (1UL << 5) | (1UL << 10);

    NVIC_SetPriority(EXTI3_IRQn, 2UL);
    NVIC_SetPriority(EXTI4_IRQn, 2UL);
    NVIC_SetPriority(EXTI9_5_IRQn, 2UL);
    NVIC_SetPriority(EXTI15_10_IRQn, 2UL);
    NVIC_EnableIRQ(EXTI3_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/**
 * อ่านและล้าง event mask อย่าง atomic ต่อ ISR; การกดปุ่มเดิมหลายครั้งก่อนอ่านจะรวมเป็นหนึ่ง bit
 */
uint32_t DrvButtonsExti_TakeEvents(void)
{
    uint32_t copy;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    copy = s_events;
    s_events = 0UL;
    __set_PRIMASK(primask);
    return copy;
}

/**
 * PB3 / D3: ล้าง pending แล้วส่ง LIGHT event
 */
void DrvButtonsExti_Line3Irq(void)
{
    EXTI->PR = (1UL << 3);
    AcceptButton(1UL, BUTTON_LIGHT_MASK);
}

/**
 * PB4 / D5: ล้าง pending แล้วส่ง AC MODE event
 */
void DrvButtonsExti_Line4Irq(void)
{
    EXTI->PR = (1UL << 4);
    AcceptButton(3UL, BUTTON_MODE_MASK);
}

/**
 * IRQ รวมสาย 5–9: ตรวจเฉพาะ EXTI5 (PB5 / D4 / FAN) ที่โครงการใช้
 */
void DrvButtonsExti_Line9To5Irq(void)
{
    if ((EXTI->PR & (1UL << 5)) != 0UL)
    {
        EXTI->PR = (1UL << 5);
        AcceptButton(2UL, BUTTON_FAN_MASK);
    }
}

/**
 * IRQ รวมสาย 10–15: ตรวจเฉพาะ EXTI10 (PA10 / D2 / AC)
 */
void DrvButtonsExti_Line15To10Irq(void)
{
    if ((EXTI->PR & (1UL << 10)) != 0UL)
    {
        EXTI->PR = (1UL << 10);
        AcceptButton(0UL, BUTTON_AC_MASK);
    }
}
