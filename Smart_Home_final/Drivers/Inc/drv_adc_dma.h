#ifndef DRV_ADC_DMA_H
#define DRV_ADC_DMA_H

#include <stdint.h>

typedef struct
{
    uint16_t temperature_raw;
    uint16_t light_raw;
    uint16_t potentiometer_raw;
    uint32_t sequence;
} AdcSamples_t;

/* ตั้งลำดับ ADC channel 0,1,4 และ DMA circular buffer 3 ค่า ขนาด halfword ผลแต่ละ index ต้องตรง temperature/light/potentiometer ใน IRQ handler */
void DrvAdcDma_Init(void);
/* เรียกจาก TIM2 ISR ทุก tick: เริ่ม scan ใหม่แล้วคืนทันที ไม่วนรอ conversion */
void DrvAdcDma_StartFromIsr(void);
/* คืน snapshot รอบล่าสุดที่ DMA ทำเสร็จ ปิด IRQ สั้น ๆ เพื่อไม่ให้ field ถูกแก้ระหว่าง copy sequence=0 หมายถึงยังไม่มีชุดข้อมูล; ไม่รับรองว่าข้อมูลใหม่ใน tick ปัจจุบัน */
AdcSamples_t DrvAdcDma_GetLatest(void);
/* เมื่อ DMA ครบ 3 ค่า copy ไป snapshot และเพิ่ม sequence; error flags ถูกล้างแต่ยังไม่มี recovery/counter */
void DrvAdcDma_IrqHandler(void);

#endif

