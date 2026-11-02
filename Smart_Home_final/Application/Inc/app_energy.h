#ifndef APP_ENERGY_H
#define APP_ENERGY_H

/* เรียกครั้งเดียวหลัง reset: พลังงานเต็มใน RAM, AC Manual OFF, LIGHT Auto static state อื่นเริ่มศูนย์จาก startup; ฟังก์ชันนี้ไม่ใช่ reset-state API สำหรับเรียกซ้ำ */
void AppEnergy_Init(void);
/* main context: รับปุ่ม/UART แล้วประมวลผล pending ticks ด้วย snapshot ADC ล่าสุด startup 20 ticks และ display test 200 ticks พักแบบจำลองพลังงาน รอบปกติ: convert -> evaluate -> energy -> evaluate -> indicators -> telemetry */
void AppEnergy_RunOnce(void);

#endif
