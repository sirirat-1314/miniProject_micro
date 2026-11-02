# คู่มืออ่านและปรับโค้ด

## อ่านตามลำดับ

| ไฟล์ | จุดสำคัญ |
|---|---|
| `../Src/main.c` | เริ่ม drivers แล้วเรียก AppEnergy_RunOnce และ WFI |
| `../Inc/project_config.h` | ค่ากำหนดร่วมและหน่วย มีคอมเมนต์แบ่งหมวด |
| `../Application/Src/app_energy.c` | EnergyState_t, HandleButtons, EvaluateOutputs, UpdateEnergy, RunOnce |
| `../Application/Src/app_temperature.c` | sensor model และสูตร NTC/LM35/TMP36 |
| `../Drivers/Src/drv_adc_dma.c` | scan 0/1/4, DMA buffer, snapshot และ IRQ |
| `../Drivers/Src/drv_uart_irq.c` | TX/RX ring buffer และ RXNE/TXE IRQ |
| `../Drivers/Src/drv_buttons_exti.c` | EXTI mapping, event mask และ debounce |
| `../Drivers/Src/drv_timer.c` | TIM2 100 ms, pending ticks และเริ่ม ADC |
| `../Drivers/Src/drv_gpio.c` | ขา LED/BCD และ GPIO init |
| `../Src/stm32f4xx_it.c` | เชื่อม vector กับ driver handler |

คอมเมนต์หน้าแต่ละฟังก์ชันอธิบายเจตนา หน่วย ผลคืน และข้อจำกัด API โดยไม่เปลี่ยนชื่อ public symbols หรือ source layout ที่ build ใช้

## สูตรและตำแหน่งแก้

| เรื่อง | สูตรปัจจุบัน | แก้ที่ |
|---|---|---|
| แสง | B=4095-raw ตาม default; I กลับทิศ | NormalizeBrightness + PHOTO_ADC_BRIGHT_HIGH |
| ไฟ Auto | B<1200 เปิด; B>1500 ปิด; ระหว่างคงเดิม | LIGHT_ON/OFF_THRESHOLD |
| target | 18+floor(raw*12/4095) °C | AppEnergy_RunOnce |
| NTC ลง GND | R=Rfixed*raw/(4095-raw) | AppTemperature_Convert |
| อุณหภูมิ NTC | T=1/(1/298.15+ln(R/R25)/Beta)-273.15 | NTC constants ใน config |
| Auto AC | valid และ tenths_c>300 | EvaluateOutputs |
| ชาร์จ | floor((B-900)*300/3195) เมื่อ B>900; มิฉะนั้น 0 | SOLAR_START_THRESHOLD, SOLAR_MAX_PER_TICK |
| โหลด | LIGHT*100 + FAN*150 + AC*(200+8*(30-target)) | constants ด้านบน app_energy.c |
| พลังงาน | clamp(E+charge-cost,0,540000) | UpdateEnergy |
| ระดับ | ceil(E/60000), cap 9; EMPTY override เป็น 0 | CalculateBatteryLevel / EvaluateOutputs |
| restart | E>=6000 units | EMPTY_RESTART_ENERGY |
| TIM2 | 16MHz/(15999+1)/(99+1)=10Hz | DrvTimer_Init / APP_TICK_MS |

ตัวอย่างชาร์จช้าลง: `SOLAR_MAX_PER_TICK` 300→150 ทำให้ปิดโหลดและแสงเต็มชาร์จ 60000 units ใน 40 s, เต็มใน 360 s, ปลด EMPTY 6000 ใน 4 s ต้องแก้ข้อความ H/help ที่ฝังตัวเลขไว้ให้ตรงด้วย

ตัวอย่าง Auto เปิด 28°C: เปลี่ยน `temperature_tenths_c > 300` เป็น `> 280` ใน EvaluateOutputs ไม่ใช่เปลี่ยน 30U ในสูตร cost

หากย้าย LED ต้องแก้ทั้ง GPIO init และ SetLed; ย้าย BCD ต้องแก้ทั้ง init/write/readback และข้อความ MAP; ย้ายปุ่มต้องแก้ GPIO, SYSCFG, EXTI และ handler ให้ครบ

## ข้อที่ต้องระวังเมื่อแก้

- target ปัจจุบันไม่เกิน 30 หากเพิ่มเกิน 30 ต้องแก้ unsigned `30U-target_c` เพื่อไม่ให้ underflow
- เปลี่ยน APP_TICK_MS ต้องปรับ cost/charge ต่อ tick, ตัวคูณ telemetry 10, startup/display-test ticks และ debounce ด้วย
- NTC_PARAMETERS_VERIFIED เปลี่ยน label ไม่ได้ calibrate หรือบล็อก Auto; ต้องวัดและยืนยันวงจรจริง
- build.ps1 ส่ง TEMP_SENSOR_MODEL ผ่าน compiler define จึง override default ใน header
- sequence!=0 หมายถึงเคยมี ADC ไม่ใช่ freshness timeout; ระบบปัจจุบันยังไม่มี watchdog ของ sample
- UART queue เต็มส่งข้อความไม่สำเร็จได้; application จงใจทิ้งผลคืนอยู่ตามฐาน R3
- X อัปเดต outputs ในรอบปกติถัดไป ต่างจาก A/L/F/M ที่ปรับ load LED ทันที
- การเปลี่ยน telemetry format ต้องแก้ parser และ SelfTest ใน live_status.ps1 พร้อมกัน

## เกณฑ์โครงงาน

GPIO ใช้ LED/BCD/input, UART ใช้ IRQ, ADC ใช้ DMA, EXTI มี 4 จุด และ Timer เป็น additional peripheral แยก Application กับ Drivers ชัดเจน ส่วน MISRA อย่างน้อย 22 rules ต้องมี check sheet และ static-analysis report เพิ่ม การ build ผ่านไม่เท่ากับ MISRA compliance

## การทดสอบหลังแก้

Build → dashboard SelfTest → ARM application tests → flash → ทดสอบปุ่ม/ADC/BCD บนบอร์ด โดยกด V ตรวจ ID, D ตรวจเลข 0–9, Z ตรวจ EMPTY แล้วส่องแสงให้สะสมถึง restart threshold

สำหรับ ARM emulator ติดตั้ง dependency ในโครงการแล้วรัน:

```powershell
python -m pip install --target tests/.deps unicorn==2.1.4
python tests/run_energy_tests.py
```

Test นี้ใช้ app_energy จริงแต่แทน drivers และ temperature conversion ด้วย stub จึงไม่ยืนยันขาจริง/ADC registers/สูตร sensor จริง
