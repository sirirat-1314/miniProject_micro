# Smart_Home_final

**เริ่มอ่าน:** [Block diagram](docs/BLOCK_DIAGRAM_TH.md) → [คู่มือโค้ดและการแก้ไข](docs/CODE_GUIDE_TH.md) → [ผลตรวจสอบ](docs/VALIDATION_TH.md)

## สิ่งที่เปลี่ยนในชุด final

- ชื่อโครงการ CubeIDE, launch และผล build เป็น `Smart_Home_final`
- เพิ่มคอมเมนต์อธิบายหน้าที่ฟังก์ชัน, pin mapping, หน่วยข้อมูล, สูตร, interrupt/main และข้อจำกัด
- จัดประวัติ README เก่าไว้ใน `docs/history` เพื่อไม่สับสนกับคู่มือปัจจุบัน
- ไม่คัดลอก Debug/Release และ firmware เก่ามาปะปน; Firmware ในชุดนี้สร้างจาก source ชุดใหม่
- เพิ่ม `libm` ใน CubeIDE ทั้ง Debug/Release สำหรับ `logf` ในสูตร NTC ให้ตรงกับ build.ps1
- ตั้งเส้นทาง log ของ debugger ตาม workspace แทนตำแหน่งเครื่องเดิม
- คง firmware ID `SMART_HOME_V1.4_R3_20261006_SEG_EMPTY` เพราะไม่ได้เปลี่ยนพฤติกรรมการควบคุม

## โครงสร้าง

```text
Smart_Home_final/
├── Application/Inc, Src     กติกาโหลด พลังงาน และแปลงอุณหภูมิ
├── Drivers/Inc, Src         GPIO, UART IRQ, ADC DMA, EXTI, TIM2, clock, BCD
├── Inc/project_config.h    ค่า threshold, sensor model, timing, firmware ID
├── Src/                    main, IRQ entry points และ runtime support
├── Startup/                vector table และ startup ของ MCU
├── CMSIS/                  vendor headers (คงเดิม)
├── docs/                   คู่มือ แผนภาพ ผลตรวจสอบ และประวัติ
├── tests/                  application regression test และ ARM runner
├── Firmware/               ELF/HEX/MAP ที่ build ใหม่ พร้อม object files
├── .project / .cproject    Import เป็น STM32CubeIDE project
├── Smart_Home_final.launch Debug configuration ของชุดนี้
├── build.ps1               Build ด้วย ARM GCC ที่ติดตั้งใต้ C:/ST
├── live_status.ps1         หน้าจอ PC และ parser self-test
└── Open_Live_Status.cmd    เปิดหน้าจอ PC
```

## Build และใช้งาน

เปิด PowerShell ภายในโฟลเดอร์นี้:

```powershell
./build.ps1 -TempModel 3
./live_status.ps1 -SelfTest
./live_status.ps1 -Port COM3
```

เปลี่ยน COM3 ให้ตรงบอร์ดจริง และปิด terminal อื่นที่ใช้พอร์ตนั้น ผล build คือ `Firmware/Smart_Home_final.elf` และ `Firmware/Smart_Home_final.hex` ต้อง flash เองก่อนดูผลบนบอร์ด เปิด terminal ใหม่อย่างเดียวไม่เปลี่ยน firmware

สำหรับ CubeIDE: File → Import → Existing Projects into Workspace → เลือกโฟลเดอร์นี้ → เลือก `Smart_Home_final` → Build โครงการนี้ → ใช้ `Smart_Home_final.launch` สำหรับ Debug ผล build ของ IDE อยู่ `Debug/` หรือ `Release/` ซึ่งแยกจาก `Firmware/` ของ PowerShell ไม่ควรเลือก launch ของรุ่นเก่า

ไฟล์ source เป็น UTF-8 เพื่อแสดงคอมเมนต์ภาษาไทย หาก editor แสดงอักษรผิดให้เลือก UTF-8 ไม่ต้องแก้ encoding ของ CMSIS

`build.ps1 -TempModel` เลือก 0=UNKNOWN, 1=LM35, 2=TMP36, 3=NTC ค่าเริ่มเป็น 3 และ override default ใน header; CubeIDE ใช้ default ใน header เมื่อไม่ได้กำหนด symbol เพิ่ม

## คำสั่งและพฤติกรรม

| คำสั่ง | หน้าที่ |
|---|---|
| A / 1 | แอร์ Manual เปิด/ปิด |
| L / 2 | ไฟ Auto → ON → OFF |
| F / 3 | พัดลมเปิด/ปิด |
| M / 4 | แอร์ Auto/Manual |
| X / 0 | ขอปิดโหลดทั้งหมด; มีผลในรอบประมวลผลปกติถัดไป |
| I | กลับทิศแสงชั่วคราว |
| Z | จำลองแบตหมดและล็อกโหลด |
| D | ทดสอบเลขจอ 0–9 รวม 20 s; พักพลังงานระหว่างทดสอบ |
| V / ? / H | รุ่น / สถานะ / วิธีชาร์จ |
| Q | ปิดโปรแกรม PC ไม่ได้ปิด MCU |

NORMAL ระดับ 4–9; เข้า ECO ระดับ 1–3 จะตัด AC/FAN หนึ่งครั้งแต่เปิดกลับได้; EMPTY เมื่อพลังงานศูนย์จะปิดทุกโหลดและจอเป็น 0 จนสะสมถึง 6000 units แล้วต้องสั่งโหลดใหม่

**ข้อจำกัดที่คงจาก R3:** LED แทนโหลดจริง, แบต/solar เป็นแบบจำลอง units ใน RAM, reset เริ่มเต็ม, NTC ยังเป็นค่าประมาณ และ Auto AC ใช้เกณฑ์ >30°C คงที่ ส่วน target 18–30°C ใช้คำนวณ cost ไม่ใช่ thermostat setpoint ไม่มีผลยืนยัน MISRA compliance หรือฮาร์ดแวร์จริงจากการจัดชุดนี้
