#ifndef DRV_CLOCK_H
#define DRV_CLOCK_H

/* เลือก HSI 16 MHz จริงใน register; การแก้ CPU_CLOCK_HZ อย่างเดียวไม่เปลี่ยน clock ฮาร์ดแวร์ หมายเหตุ: wait ตอน init ยังไม่มี timeout ไม่ใช่ ADC/UART data polling */
void DrvClock_Init(void);

#endif

