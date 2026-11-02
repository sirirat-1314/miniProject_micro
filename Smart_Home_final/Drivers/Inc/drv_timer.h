#ifndef DRV_TIMER_H
#define DRV_TIMER_H

#include <stdint.h>

/* PSC ลด counter เหลือ 1 kHz; ARR กำหนด APP_TICK_MS; เปิด update interrupt */
void DrvTimer_Init(void);
/* คืนเวลา software หน่วย ms แต่ความละเอียดเป็นหนึ่ง APP_TICK_MS ไม่ใช่ 1 ms */
uint32_t DrvTimer_GetMs(void);
/* main อ่านจำนวน tick ค้างและล้างใน critical section เพื่อไม่ให้ ISR increment สูญหาย */
uint32_t DrvTimer_TakeTicks(void);
/* เพิ่มเวลา/งานค้างและเริ่ม ADC; จำกัด pending ticks 1000 เพื่อไม่สะสมไม่สิ้นสุด ไม่ทำ temperature conversion หรือ UART formatting ใน ISR */
void DrvTimer_IrqHandler(void);

#endif

