/**
 * @file drv_sevenseg.c
 * @brief แสดงระดับแบต 0–9 ผ่าน BCD decoder; ไม่ใช้ multiplex และไม่ได้ขับ segment a–g โดยตรง
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "drv_sevenseg.h"
#include "drv_gpio.h"

/**
 * ส่งระดับแบตเป็น BCD 0–9; now_ms สงวนไว้แต่รุ่นนี้ยังไม่ใช้กระพริบ
 */
void DrvSevenSeg_ShowBattery(uint8_t level, uint32_t now_ms)
{
    /* ตอนนี้ decoder เป็น 4-bit (0-9) เต็มรูปแบบ แสดงตัวเลขตรงกับระดับแบตได้เลย
       ไม่ต้องกระพริบแทนเลข 8/9 อีกต่อไป; now_ms เก็บไว้เผื่ออนาคต (เช่น blink ตอนแบตต่ำ) */
    (void)now_ms;
    const uint8_t code = (level > 9U) ? 9U : level;
    DrvGpio_SetSevenSegCode(code);
}
