#ifndef APP_TEMPERATURE_H
#define APP_TEMPERATURE_H
#include <stdbool.h>
#include <stdint.h>
/* คืน true เมื่อแปลง raw สำเร็จและเขียน tenths_c หน่วย 0.1°C; false เมื่อผิดช่วง/null NTC: 1/T = 1/298.15 + ln(R/R25)/Beta; LM35/TMP36 แปลงตาม voltage model มี floating point ในฟังก์ชันนี้ แม้ state ที่ application เก็บเป็น integer */
bool AppTemperature_Convert(uint16_t raw, int16_t *tenths_c);
/* คืนชื่อ model ที่เลือกตอน compile; ESTIMATE เป็น label ไม่ได้ปิดการควบคุม Auto */
const char *AppTemperature_Model(void);
#endif
