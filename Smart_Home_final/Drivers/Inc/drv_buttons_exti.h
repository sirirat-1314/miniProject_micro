#ifndef DRV_BUTTONS_EXTI_H
#define DRV_BUTTONS_EXTI_H

#include <stdint.h>

#define BUTTON_AC_MASK       (1UL << 0)
#define BUTTON_LIGHT_MASK    (1UL << 1)
#define BUTTON_FAN_MASK      (1UL << 2)
#define BUTTON_MODE_MASK     (1UL << 3)

/* เลือก port ของ EXTI3/4/5/10 แล้วเปิด falling edge และ NVIC; ปุ่มกด active-low */
void DrvButtonsExti_Init(void);
/* อ่านและล้าง event mask อย่าง atomic ต่อ ISR; การกดปุ่มเดิมหลายครั้งก่อนอ่านจะรวมเป็นหนึ่ง bit */
uint32_t DrvButtonsExti_TakeEvents(void);
/* PB3 / D3: ล้าง pending แล้วส่ง LIGHT event */
void DrvButtonsExti_Line3Irq(void);
/* PB4 / D5: ล้าง pending แล้วส่ง AC MODE event */
void DrvButtonsExti_Line4Irq(void);
/* IRQ รวมสาย 5–9: ตรวจเฉพาะ EXTI5 (PB5 / D4 / FAN) ที่โครงการใช้ */
void DrvButtonsExti_Line9To5Irq(void);
/* IRQ รวมสาย 10–15: ตรวจเฉพาะ EXTI10 (PA10 / D2 / AC) */
void DrvButtonsExti_Line15To10Irq(void);

#endif

