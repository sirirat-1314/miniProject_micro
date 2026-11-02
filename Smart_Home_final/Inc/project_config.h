#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include <stdint.h>

/* Clock/serial/timebase: หากเปลี่ยน tick ต้องปรับ rates และช่วงทดสอบใน application ด้วย */
#define CPU_CLOCK_HZ             (16000000UL)
#define UART_BAUD_RATE            (115200UL)
#define APP_TICK_MS               (100UL)
#define BUTTON_DEBOUNCE_MS        (150UL)
#define UART_TX_BUFFER_SIZE       (2048U)

/* ADC 12-bit; reference ที่สมมติคือ 3.3 V; thresholds ใช้ brightness ไม่ใช่ raw LDR */
#define ADC_FULL_SCALE            (4095UL)
#define ADC_REFERENCE_MV          (3300UL)
#define LIGHT_ON_THRESHOLD        (1200U)
#define LIGHT_OFF_THRESHOLD       (1500U)
#define SOLAR_START_THRESHOLD     (900U)

/* 1 = ค่า ADC สูงหมายถึงแสงมาก; เปลี่ยนเป็น 0 หากวงจรแบ่งแรงดันกลับด้าน */
#define PHOTO_ADC_BRIGHT_HIGH     (0U)

/* ต้องยืนยันชนิดเซนเซอร์ก่อนแสดงองศา ไม่ใช้ค่า offset เพื่อบังคับอุณหภูมิห้อง */
#define TEMP_MODEL_UNKNOWN 0U
#define TEMP_MODEL_LM35    1U
#define TEMP_MODEL_TMP36   2U
#define TEMP_MODEL_NTC    3U
#ifndef TEMP_SENSOR_MODEL
#define TEMP_SENSOR_MODEL TEMP_MODEL_NTC
#endif
/* สำหรับ NTC เท่านั้น: ต้องกรอกค่าจากอุปกรณ์/วงจรจริงก่อนเลือกโมเดล NTC */
#define NTC_R25_OHMS       (10000.0f)
#define NTC_FIXED_OHMS     (10000.0f)
#define NTC_BETA_K         (3950.0f)
#define NTC_TO_GROUND      (1U)
#define NTC_PARAMETERS_VERIFIED (0U) /* 0=ค่าประมาณ รอยืนยัน R25/Beta/ทิศวงจร */
/* พลังงานจำลองต่อ tick; ไม่ใช่วงจรชาร์จจริง และไม่คงค่าเมื่อ reset */
#define SOLAR_MAX_PER_TICK (300UL) /* แสงเต็ม: 60000/(300*10) = 20 วินาที/ระดับ */
#define EMPTY_RESTART_ENERGY (6000UL) /* 2 seconds at maximum solar; no loads */
/* คง ID ฐาน R3 เพราะชุด final เปลี่ยน comments/packaging ไม่เปลี่ยน control logic */
#define FIRMWARE_ID "SMART_HOME_V1.4_R3_20261006_SEG_EMPTY"
#endif

