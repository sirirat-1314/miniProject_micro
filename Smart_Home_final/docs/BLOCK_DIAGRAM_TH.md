# Block diagram — Smart_Home_final

ลูกศรแสดงข้อมูลและคำสั่ง ไม่ใช่เส้นทางจ่ายไฟ แผนภาพตรงกับพฤติกรรมฐาน 1.4 R3

```mermaid
flowchart TD
    NTC["NTC: PA0"] --> ADC
    LDR["LDR: PA1"] --> ADC
    POT["Potentiometer: PA4"] --> ADC
    BTN["ปุ่ม AC/LIGHT/FAN/MODE<br/>PA10/PB3/PB5/PB4"] --> EXTI
    PC["PC Live Status"] <-->|"115200 baud"| UART
    subgraph MCU["STM32F411RE"]
      ADC["ADC1 scan 3 ช่อง"] --> DMA["DMA2 Stream0 + IRQ"]
      DMA -->|"snapshot ใน RAM"| APP["Application<br/>Auto/Manual และพลังงาน"]
      EXTI["EXTI + debounce"] -->|"event mask"| APP
      UART["USART2 RX/TX Interrupt<br/>PA2 TX / PA3 RX"] <-->|"คำสั่ง / telemetry"| APP
      TIMER["TIM2 IRQ ทุก 100 ms"] -->|"SWSTART"| ADC
      TIMER -->|"pending ticks"| APP
      APP <--> ENERGY["พลังงานจำลองใน RAM<br/>NORMAL / ECO / EMPTY"]
      APP --> GPIO["GPIO Output"]
    end
    GPIO --> LED["LED LIGHT/AC/FAN/ECO<br/>PA5/PA6/PA7/PB6"]
    GPIO --> BCD["BCD: PC7, PA8, PB10, PA9"]
    BCD --> DEC["BCD decoder"] --> SEG["7-segment ระดับ 0–9"]
```

## หน้าที่และเส้นทางข้อมูล

1. TIM2 เริ่ม scan ADC ทุก 100 ms; DMA ส่งค่าทั้งสามลง RAM และ interrupt บันทึก snapshot
2. EXTI รับการกดปุ่ม ส่วน USART2 รับคำสั่ง PC ลงคิว ทั้งคู่ส่งให้ application ตัดสินใจใน main
3. Application แปลง ADC เป็นอุณหภูมิ/ความสว่าง/target แล้วประเมินโหลดก่อนและหลังอัปเดตพลังงาน
4. GPIO ขับ LED และรหัส BCD ตาม state; telemetry กลับ PC ผ่านคิว TX และ UART interrupt

```mermaid
flowchart TD
    E["รับปุ่ม / UART"] --> T{"มี tick ค้าง?"}
    T -->|"ไม่มี"| W["WFI รอ interrupt"]
    T -->|"มี"| A["อ่าน ADC snapshot ล่าสุด"]
    A --> C["แปลง sensor และ target"]
    C --> O["EvaluateOutputs ก่อนคิดพลังงาน"]
    O --> P["E ใหม่ = clamp E + charge - cost"]
    P --> S["EvaluateOutputs หลังคิดพลังงาน<br/>ตรวจ ECO / EMPTY"]
    S --> G["LED / 7-segment / telemetry"]
    G --> T
```

แผนภาพล่างเป็นรอบปกติ ไม่รวม startup test 2 s และ display test 20 s ที่พักพลังงาน ค่า ADC เป็นชุดล่าสุดที่ทำเสร็จ ไม่ได้รอชุดใหม่ใน tick เดียวกัน และ UART/ADC ไม่มี loop รอรับ sample แบบ polling

แสงน้อยทำให้ชาร์จน้อยและไฟ Auto อาจเปิด จึงเร่งการลดแบตจำลอง เมื่อผ่าน 4→3 เข้า ECO ตัดแอร์/พัดลมครั้งเดียว เมื่อ E=0 เข้า EMPTY และรอสะสมถึง 6000 units ก่อนปลดล็อก
