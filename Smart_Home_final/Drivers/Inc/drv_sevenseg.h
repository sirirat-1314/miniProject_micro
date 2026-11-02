#ifndef DRV_SEVENSEG_H
#define DRV_SEVENSEG_H

#include <stdint.h>

/* ส่งระดับแบตเป็น BCD 0–9; now_ms สงวนไว้แต่รุ่นนี้ยังไม่ใช้กระพริบ */
void DrvSevenSeg_ShowBattery(uint8_t level, uint32_t now_ms);

#endif

