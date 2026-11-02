#ifndef DRV_UART_IRQ_H
#define DRV_UART_IRQ_H

#include <stdbool.h>
#include <stdint.h>

/* ตั้ง PA2=TX, PA3=RX, AF7 และ USART2 115200 8N1 ตาม config เปิด RX interrupt ตั้งแต่เริ่ม; TX interrupt เปิดเมื่อมีข้อมูลให้ส่ง */
void DrvUart_Init(void);
/* คัดลอกข้อความลง TX queue ไม่รอ hardware ส่งจบ; false เมื่อ null หรือพื้นที่ไม่พอ การสำรวจคิวอยู่ใน critical section; application ปัจจุบันทิ้งผลคืน จึงอาจเสียข้อความเมื่อเต็ม */
bool DrvUart_Write(const char *text);
/* แปลง unsigned 32-bit เป็น decimal สูงสุด 10 หลักและ null terminator แล้วส่งเข้าคิว */
bool DrvUart_WriteU32(uint32_t value);
/* อ่าน byte ที่ ISR รับไว้ใน RAM; false เมื่อคิวว่าง ไม่ใช่ polling รอ USART register */
bool DrvUart_ReadByte(uint8_t *value);
/* RXNE/ORE: อ่าน DR และเก็บ byte ถ้าคิวไม่เต็ม; TXE: ส่งหนึ่ง byte แล้วปิด TXEIE เมื่อคิวว่าง */
void DrvUart_IrqHandler(void);

#endif

