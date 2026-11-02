#ifndef DRV_GPIO_H
#define DRV_GPIO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DEVICE_LED_LIGHT = 0,
    DEVICE_LED_AC,
    DEVICE_LED_FAN,
    DEVICE_LED_ECO
} DeviceLed_t;

/* เปิด clock port A/B/C แล้วตั้ง PA0/1/4 analog, ปุ่ม pull-up และ LED/BCD output BCD bit 8 ของ R3 ใช้ PA9; ตรวจสายจริงบน shield ก่อนเปลี่ยน mapping */
void DrvGpio_Init(void);
/* แปลงชื่อโหลดเป็นขา: LIGHT=PA5, AC=PA6, FAN=PA7, ECO=PB6 BSRR ตั้ง/ล้างบิตโดยไม่ read-modify-write ODR */
void DrvGpio_SetLed(DeviceLed_t led, bool on);
/* ส่งเลขฐานสิบเป็น BCD: น้ำหนัก 1=PC7, 2=PA8, 4=PB10, 8=PA9; clamp เกิน 9 */
void DrvGpio_SetSevenSegCode(uint8_t code);
/* อ่านระดับขา MCU กลับเป็นรหัส BCD; ไม่ใช่การอ่านหรือยืนยันตัวเลขบนจอจริง */
uint8_t DrvGpio_GetSevenSegPins(void);

#endif


