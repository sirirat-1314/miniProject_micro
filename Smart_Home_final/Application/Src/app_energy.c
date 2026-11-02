/**
 * @file app_energy.c
 * @brief กติกา Smart Home: Auto/Manual, พลังงานจำลอง units, ECO และ EMPTY; ติดต่อ hardware ผ่าน Drv APIs
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "app_energy.h"
#include "drv_adc_dma.h"
#include "drv_buttons_exti.h"
#include "drv_gpio.h"
#include "app_temperature.h"

#include "drv_sevenseg.h"
#include "drv_timer.h"
#include "drv_uart_irq.h"
#include "project_config.h"
#include <stdbool.h>
#include <stdint.h>

/* หน่วยพลังงานละเอียดขึ้นเพื่อสะสมค่าทีละ 100 ms โดยไม่สูญเสียเศษจากการหาร */
#define ENERGY_PER_LEVEL       (60000UL)
#define ENERGY_MAX             (9UL * ENERGY_PER_LEVEL)
#define LIGHT_COST_PER_TICK    (100UL) /* 1 ขั้น / 60 วินาที เมื่อไม่มีแสง */
#define FAN_COST_PER_TICK      (150UL) /* 1 ขั้น / 40 วินาที เมื่อไม่มีแสง */
#define AC_BASE_COST_PER_TICK  (200UL) /* ที่ 30°C: 1 ขั้น / 30 วินาที */
#define AC_COLD_EXTRA_PER_DEG  (8UL)   /* ที่ 18°C: ประมาณ 20.3 วินาที/ขั้น */

typedef enum
{
    LIGHT_MODE_AUTO = 0,
    LIGHT_MODE_MANUAL_ON,
    LIGHT_MODE_MANUAL_OFF
} LightMode_t;

/* สถานะหลักใน RAM; energy ไม่ใช่ประจุแบตที่วัดจริง และสูญหายเมื่อ reset */
typedef struct
{
    uint32_t energy;                /* 0..540000 units, ไม่ใช่ mAh */
    uint32_t charge_per_tick;
    uint32_t cost_per_tick;
    int16_t temperature_tenths_c;
    bool temperature_valid;
    uint8_t target_c;               /* 18..30°C ใช้กำหนด cost ของแอร์ */
    uint8_t battery_level;
    bool ac_auto_mode;
    bool ac_manual_request;
    bool ac_auto_latch;
    bool ac_on;
    bool light_on;
    bool fan_request;
    bool fan_on;
    bool power_saving;
    bool empty_lock;                /* จอเป็น 0 จนสะสมถึง restart threshold */
    LightMode_t light_mode;
} EnergyState_t;

static EnergyState_t s_state;
static uint32_t s_telemetry_ticks;
static bool s_force_telemetry;
static bool s_invert_light;
static uint32_t s_startup_ticks;
static uint32_t s_display_test_ticks;

/**
 * แปลง LDR raw เป็นความสว่าง 0–4095 (ไม่ใช่ lux)
 * default PHOTO_ADC_BRIGHT_HIGH=0: B=4095-raw; คำสั่ง I กลับทิศชั่วคราวใน RAM
 */
static uint16_t NormalizeBrightness(uint16_t raw)
{
#if PHOTO_ADC_BRIGHT_HIGH == 1U
    return s_invert_light ? (uint16_t)(ADC_FULL_SCALE - raw) : raw;
#else
    return s_invert_light ? raw : (uint16_t)(ADC_FULL_SCALE - raw);
#endif
}

/**
 * ปัดระดับขึ้น: ceil(energy/60000), จำกัดสูงสุด 9; EMPTY lock อาจ override ให้แสดง 0
 */
static uint8_t CalculateBatteryLevel(uint32_t energy)
{
    uint32_t level = (energy + ENERGY_PER_LEVEL - 1UL) / ENERGY_PER_LEVEL;
    /* พลังงานศูนย์ต้องแสดงระดับศูนย์จริง ไม่ปัดขึ้นเป็นหนึ่ง */
    if (level > 9UL)
    {
        level = 9UL;
    }
    return (uint8_t)level;
}

static void EvaluateOutputs(uint16_t brightness);

/**
 * ใช้ handler เดียวกันสำหรับ EXTI และ UART A/L/F/M; ประเมิน ECO entry ก่อนรับคำสั่งใหม่
 * EMPTY บล็อกคำสั่งโหลด; เมื่อรับสำเร็จอัปเดต load LEDs ทันทีไม่รอ tick
 */
static void HandleButtons(uint32_t events)
{
    /* Apply a pending Eco entry before accepting a fresh user request. */
    if (events != 0UL)
    {
        s_state.battery_level = CalculateBatteryLevel(s_state.energy);
        EvaluateOutputs(NormalizeBrightness(DrvAdcDma_GetLatest().light_raw));
    }
    /* ใช้จุดตรวจเดียวกันสำหรับปุ่มจริงและ keyboard ไม่เก็บคำสั่งเปิดค้างเมื่อแบตหมด */
    if (s_state.empty_lock && (events != 0UL))
    {
        (void)DrvUart_Write("[BLOCKED] BATTERY EMPTY: all loads OFF. Recharge first.\r\n");
        s_force_telemetry = true;
        return;
    }
    if ((events & BUTTON_AC_MASK) != 0UL)
    {
        /* กด AC แล้วเข้า manual โดยตรง เพื่อให้คำสั่งผู้ใช้มีผลชัดเจน */
        s_state.ac_manual_request = s_state.ac_auto_mode ? !s_state.ac_on : !s_state.ac_manual_request;
        s_state.ac_auto_mode = false;
        (void)DrvUart_Write("[BUTTON D2] AC manual request changed\r\n");
    }
    if ((events & BUTTON_LIGHT_MASK) != 0UL)
    {
        (void)DrvUart_Write("[BUTTON D3] LIGHT mode changed\r\n");
        if (s_state.light_mode == LIGHT_MODE_AUTO)
        {
            s_state.light_mode = LIGHT_MODE_MANUAL_ON;
        }
        else if (s_state.light_mode == LIGHT_MODE_MANUAL_ON)
        {
            s_state.light_mode = LIGHT_MODE_MANUAL_OFF;
        }
        else
        {
            s_state.light_mode = LIGHT_MODE_AUTO;
        }
    }
    if ((events & BUTTON_FAN_MASK) != 0UL)
    {
        (void)DrvUart_Write("[BUTTON D4] FAN request changed\r\n");
        s_state.fan_request = !s_state.fan_request;
    }
    if ((events & BUTTON_MODE_MASK) != 0UL)
    {
        (void)DrvUart_Write("[BUTTON D5] AC AUTO/MANUAL changed\r\n");
        s_state.ac_auto_mode = !s_state.ac_auto_mode;
    }
    if (events != 0UL)
    {
        /* Commands update outputs immediately, even without a timer tick. */
        EvaluateOutputs(NormalizeBrightness(DrvAdcDma_GetLatest().light_raw));
        DrvGpio_SetLed(DEVICE_LED_AC, s_state.ac_on);
        DrvGpio_SetLed(DEVICE_LED_LIGHT, s_state.light_on);
        DrvGpio_SetLed(DEVICE_LED_FAN, s_state.fan_on);
        s_force_telemetry = true;
    }
}

/**
 * ลำดับสำคัญ: EMPTY lock -> ECO entry -> Auto/Manual -> light hysteresis
 * เข้า ECO ตัด AC/FAN ครั้งเดียว เปิดใหม่ได้; EMPTY ปลดเมื่อ energy >= 6000 ตาม config
 * Auto AC เปิดที่ >30°C คงที่ ไม่ได้เทียบ target_c ในรุ่นนี้
 */
static void EvaluateOutputs(uint16_t brightness)
{
    if (s_state.energy == 0UL) { s_state.empty_lock = true; }
    if (s_state.empty_lock && (s_state.energy >= EMPTY_RESTART_ENERGY))
    {
        s_state.empty_lock = false;
        s_force_telemetry = true;
    }
    if (s_state.empty_lock)
    {
        s_state.battery_level = 0U;
        s_force_telemetry = s_force_telemetry || s_state.ac_on || s_state.fan_on || s_state.light_on;
        /* แบตหมดตัดโหลดและล้างคำสั่งเดิม ต้องสั่งใหม่หลังชาร์จกลับ */
        s_state.ac_on = false;
        s_state.fan_on = false;
        s_state.light_on = false;
        s_state.ac_manual_request = false;
        s_state.fan_request = false;
        s_state.ac_auto_latch = false;
        s_state.ac_auto_mode = false;
        s_state.light_mode = LIGHT_MODE_MANUAL_OFF;
        s_state.power_saving = true;
        return;
    }
    s_state.battery_level = CalculateBatteryLevel(s_state.energy);

    /* ปิดโหลดหนักครั้งเดียวตอนเข้า Eco ไม่ล็อกทุก tick เหมือน 1.3 */
    const bool eco_now = (s_state.battery_level < 4U);
    if (eco_now && !s_state.power_saving)
    {
        s_state.ac_manual_request = false;
        s_state.fan_request = false;
        s_state.ac_auto_latch = false;
        /* กัน Auto เปิดกลับเองทันที ผู้ใช้กด A หรือ M เพื่อเปิด/เลือก Auto ใหม่ได้ */
        s_state.ac_auto_mode = false;
        s_force_telemetry = true;
        (void)DrvUart_Write("[ECO ENTER] AC/FAN switched OFF once. A/F/M can enable them again.\r\n");
    }
    s_state.power_saving = eco_now;
    if (s_state.ac_auto_mode)
    {
        /* ทำตาม requirement: สูงกว่า 30°C เปิด มิฉะนั้นปิด */
        if (s_state.temperature_valid && (s_state.temperature_tenths_c > 300))
        {
            s_state.ac_auto_latch = true;
        }
        else
        {
            s_state.ac_auto_latch = false;
        }
    }

    s_state.ac_on = s_state.ac_auto_mode ? s_state.ac_auto_latch : s_state.ac_manual_request;
    s_state.fan_on = s_state.fan_request;

    if (s_state.light_mode == LIGHT_MODE_AUTO)
    {
        if (brightness < LIGHT_ON_THRESHOLD)
        {
            s_state.light_on = true;
        }
        else if (brightness > LIGHT_OFF_THRESHOLD)
        {
            s_state.light_on = false;
        }
        else
        {
            /* คงสถานะเดิมใน hysteresis band */
        }
    }
    else
    {
        s_state.light_on = (s_state.light_mode == LIGHT_MODE_MANUAL_ON);
    }
}

/**
 * หนึ่งรอบ = หนึ่ง APP_TICK_MS; Q=floor((B-900)*300/3195) เมื่อ B>900
 * Cost = LIGHT*100 + FAN*150 + AC*(200+8*(30-target)); หน่วยทั้งหมด units/tick
 * จำกัด E+Q-C ให้อยู่ 0..ENERGY_MAX โดยแยก gain/loss ป้องกัน unsigned underflow
 */
static void UpdateEnergy(uint16_t brightness)
{
    uint32_t cost = 0UL;
    uint32_t charge = 0UL;

    if ((uint32_t)brightness > SOLAR_START_THRESHOLD)
    {
        charge = (((uint32_t)brightness - SOLAR_START_THRESHOLD) * SOLAR_MAX_PER_TICK) /
                 (ADC_FULL_SCALE - SOLAR_START_THRESHOLD);
    }
    if (s_state.light_on)
    {
        cost += LIGHT_COST_PER_TICK;
    }
    if (s_state.fan_on)
    {
        cost += FAN_COST_PER_TICK;
    }
    if (s_state.ac_on)
    {
        cost += AC_BASE_COST_PER_TICK;
        cost += ((uint32_t)(30U - s_state.target_c) * AC_COLD_EXTRA_PER_DEG);
    }

    s_state.charge_per_tick = charge;
    s_state.cost_per_tick = cost;
    /* คิดสุทธิก่อนจำกัดช่วง: แบตเต็มและแสงพอเลี้ยงโหลดต้องยังคงเต็ม */
    if (charge >= cost)
    {
        const uint32_t gain = charge - cost;
        s_state.energy += (gain > (ENERGY_MAX - s_state.energy)) ?
                          (ENERGY_MAX - s_state.energy) : gain;
    }
    else
    {
        const uint32_t loss = cost - charge;
        s_state.energy -= (loss > s_state.energy) ? s_state.energy : loss;
    }
    s_state.battery_level = CalculateBatteryLevel(s_state.energy);
}

/**
 * สะท้อน state ไป LED และจอ; ECO LED ดับเมื่อ EMPTY แม้ power_saving ภายในเป็น true
 */
static void ApplyIndicators(void)
{
    DrvGpio_SetLed(DEVICE_LED_AC, s_state.ac_on);
    DrvGpio_SetLed(DEVICE_LED_LIGHT, s_state.light_on);
    DrvGpio_SetLed(DEVICE_LED_FAN, s_state.fan_on);
    /* แบตศูนย์ให้ LED ทั้งสี่ดับ แต่ MCU/UART ยังรับพลังงานและคำสั่งได้ */
    DrvGpio_SetLed(DEVICE_LED_ECO, s_state.power_saving && !s_state.empty_lock);
    DrvSevenSeg_ShowBattery(s_state.battery_level, DrvTimer_GetMs());
}

/**
 * รูปแบบข้อความ ON/OFF คงที่สำหรับ parser ฝั่ง PC
 */
static void PrintOnOff(bool on)
{
    (void)DrvUart_Write(on ? "ON" : "OFF");
}

/**
 * ส่งสถานะเข้าคิว UART; รูปแบบแต่ละแถวต้องตรง parser ใน live_status.ps1
 * charge/cost เป็นค่าจาก tick ล่าสุด ส่วน SOC มาจากพลังงานจริงที่จำลอง
 * ตัวเลข capacity และตัวคูณ 10 บางจุดฝังไว้ ต้องแก้เมื่อเปลี่ยน capacity/tick
 */
static void PrintTelemetry(const AdcSamples_t *adc)
{
    (void)DrvUart_Write("\r\n--- LIVE STATUS ---\r\n");
    (void)DrvUart_Write("FW=" FIRMWARE_ID "\r\n" );
    (void)DrvUart_Write("POLICY=ECO_ENTRY_OFF_REOPEN_ALLOWED\r\n");
    (void)DrvUart_Write("TEMP SENSOR=");
    (void)DrvUart_Write(AppTemperature_Model());
    (void)DrvUart_Write(s_state.temperature_valid ? " VALID\r\n" : " UNVERIFIED/INVALID; AUTO AC OFF\r\n");
    (void)DrvUart_Write("SEG CMD=");
    (void)DrvUart_WriteU32(s_state.battery_level);
    (void)DrvUart_Write(" GPIO=");
    (void)DrvUart_WriteU32(DrvGpio_GetSevenSegPins());
    (void)DrvUart_Write(" MAP=PC7,PA8,PB10,PA9\r\n");
    (void)DrvUart_Write("BAT=");
    (void)DrvUart_WriteU32(s_state.battery_level);
    (void)DrvUart_Write("/9 energy=");
    (void)DrvUart_WriteU32(s_state.energy);
    /* แสดงเปอร์เซ็นต์ทศนิยมหนึ่งตำแหน่ง เพื่อเห็นการสะสมก่อนเปลี่ยนระดับ */
    (void)DrvUart_Write("/540000 SOC=");
    (void)DrvUart_WriteU32((s_state.energy * 100UL) / ENERGY_MAX);
    (void)DrvUart_Write(".");
    (void)DrvUart_WriteU32(((s_state.energy * 1000UL) / ENERGY_MAX) % 10UL);
    (void)DrvUart_Write("%");
    (void)DrvUart_Write(" temp=");
    if (s_state.temperature_valid)
    {
        const int32_t value = s_state.temperature_tenths_c;
        const uint32_t magnitude = (uint32_t)((value < 0) ? -value : value);
        if (value < 0) { (void)DrvUart_Write("-"); }
        (void)DrvUart_WriteU32(magnitude / 10UL);
        (void)DrvUart_Write(".");
        (void)DrvUart_WriteU32(magnitude % 10UL);
    }
    else { (void)DrvUart_Write("NA"); }
    (void)DrvUart_Write("C target=");
    (void)DrvUart_WriteU32(s_state.target_c);
    (void)DrvUart_Write("C AC[");
    (void)DrvUart_Write(s_state.ac_auto_mode ? "AUTO," : "MANUAL,");
    PrintOnOff(s_state.ac_on);
    (void)DrvUart_Write("] LIGHT[");
    if (s_state.light_mode == LIGHT_MODE_AUTO)
    {
        (void)DrvUart_Write("AUTO,");
    }
    else
    {
        (void)DrvUart_Write("MANUAL,");
    }
    PrintOnOff(s_state.light_on);
    (void)DrvUart_Write("] FAN=");
    PrintOnOff(s_state.fan_on);
    (void)DrvUart_Write(" ECO=");
    PrintOnOff(s_state.power_saving && !s_state.empty_lock);
    (void)DrvUart_Write(" charge/cost=");
    (void)DrvUart_WriteU32(s_state.charge_per_tick);
    (void)DrvUart_Write("/");
    (void)DrvUart_WriteU32(s_state.cost_per_tick);
    (void)DrvUart_Write(" ADC(T,L,P)=");
    (void)DrvUart_WriteU32(adc->temperature_raw);
    (void)DrvUart_Write(",");
    (void)DrvUart_WriteU32(adc->light_raw);
    (void)DrvUart_Write(",");
    (void)DrvUart_WriteU32(adc->potentiometer_raw);
    (void)DrvUart_Write("\r\n");
    (void)DrvUart_Write("BATTERY: ");
    if (s_state.empty_lock)
    {
        (void)DrvUart_Write("EMPTY / ALL LOADS LOCKED");
    }
    else if (s_state.energy == ENERGY_MAX)
    {
        (void)DrvUart_Write("FULL");
    }
    else if (s_state.charge_per_tick > s_state.cost_per_tick)
    {
        (void)DrvUart_Write("CHARGING (energy increasing)");
    }
    else if (s_state.charge_per_tick < s_state.cost_per_tick)
    {
        (void)DrvUart_Write("DISCHARGING (loads exceed solar)");
    }
    else
    {
        (void)DrvUart_Write("IDLE / BALANCED");
    }
    (void)DrvUart_Write(" | Solar input=");
    (void)DrvUart_WriteU32(s_state.charge_per_tick * 10UL);
    (void)DrvUart_Write(" units/s | Load=");
    (void)DrvUart_WriteU32(s_state.cost_per_tick * 10UL);
    (void)DrvUart_Write(" units/s\r\nBrightness=");
    (void)DrvUart_WriteU32(NormalizeBrightness(adc->light_raw));
    (void)DrvUart_Write(" (solar starts above 900); DMA samples=");
    (void)DrvUart_WriteU32(adc->sequence);
    (void)DrvUart_Write(s_invert_light ? " | LDR inverted\r\n" : " | LDR normal\r\n");
    if (s_state.empty_lock)
    {
        (void)DrvUart_Write("POWER SAVING: EMPTY - all loads locked until recharge.\r\n");
    }
    else if (s_state.power_saving)
    {
        (void)DrvUart_Write("POWER SAVING: ECO - AC/FAN can be reopened; normal energy cost applies.\r\n");
    }
    if (adc->sequence == 0UL)
    {
        (void)DrvUart_Write("ADC ERROR: no DMA samples received yet.\r\n");
    }
}

/**
 * เรียกครั้งเดียวหลัง reset: พลังงานเต็มใน RAM, AC Manual OFF, LIGHT Auto
 * static state อื่นเริ่มศูนย์จาก startup; ฟังก์ชันนี้ไม่ใช่ reset-state API สำหรับเรียกซ้ำ
 */
void AppEnergy_Init(void)
{
    /* เริ่มต้นแบตเต็มทุกครั้งที่ Reset; พลังงานเก็บใน RAM เท่านั้น */
    s_state.energy = ENERGY_MAX;
    s_state.battery_level = 9U;
    s_state.target_c = 24U;
    s_state.ac_auto_mode = false;
    s_state.light_mode = LIGHT_MODE_AUTO;
    (void)DrvUart_Write("\r\n" FIRMWARE_ID "\r\n");
    (void)DrvUart_Write("ECO: switch AC/FAN OFF on entry; reopen with A/F/M. EMPTY still locks all loads.\r\n");
    (void)DrvUart_Write("Keyboard: A/1=AC L/2=light F/3=fan M/4=mode X/0=all OFF; lowercase also works. No Enter needed.\r\n");
    (void)DrvUart_Write("Battery stored in RAM; reset starts at 100%. Light=60s Fan=40s AC=20-30s per level.\r\n");
    (void)DrvUart_Write("d=display pin test (20s; energy paused); h=charging help\r\n");
    (void)DrvUart_Write("7SEG Nucleo D8=PA9: 1=PC7 2=PA8 4=PB10 8=PA9. D=test.\r\n");
    (void)DrvUart_Write("D2=AC, D3=LIGHT AUTO/ON/OFF, D4=FAN, D5=AC AUTO/MANUAL, ?=status\r\n");
    (void)DrvUart_Write("Terminal keys: a=AC l=LIGHT f=FAN m=MODE i=invert LDR x=all loads OFF\r\n");
    (void)DrvUart_Write("Charge: X, brightness above 900; max 20s/level or 180s empty-to-full. V=version.\r\n");
}

/**
 * main context: รับปุ่ม/UART แล้วประมวลผล pending ticks ด้วย snapshot ADC ล่าสุด
 * startup 20 ticks และ display test 200 ticks พักแบบจำลองพลังงาน
 * รอบปกติ: convert -> evaluate -> energy -> evaluate -> indicators -> telemetry
 */
void AppEnergy_RunOnce(void)
{
    uint32_t ticks = DrvTimer_TakeTicks();
    uint8_t received;

    HandleButtons(DrvButtonsExti_TakeEvents());
    while (DrvUart_ReadByte(&received))
    {
        /* รับได้ทั้งตัวพิมพ์ใหญ่และเล็ก รวมถึงเลขที่กดสะดวกบน keyboard */
        if ((received >= (uint8_t)'A') && (received <= (uint8_t)'Z'))
        {
            received = (uint8_t)(received + 32U);
        }
        if (received == (uint8_t)'?')
        {
            s_force_telemetry = true;
        }
        /* คำสั่งรับจาก RX interrupt ช่วยแยกปัญหาปุ่มออกจากปัญหา LED */
        switch (received)
        {
            case (uint8_t)'v':
                (void)DrvUart_Write("\r\nFW=" FIRMWARE_ID "\r\n");

                break;

            case (uint8_t)'z':
                /* คำสั่งทดสอบชัดเจน: จำลองแบตหมด ไม่ต้องรอคายประจุเก้านาที */
                s_state.energy = 0UL;
                s_state.battery_level = 0U;
                s_startup_ticks = 20UL;
                s_display_test_ticks = 0UL;
                EvaluateOutputs(0U);
                ApplyIndicators();
                s_force_telemetry = true;
                (void)DrvUart_Write("[TEST] Battery forced to ZERO. All load requests cleared. Solar can recharge.\r\n");
                break;
            case (uint8_t)'d':
                /* Diagnostic codes 0-9 remain available while battery is empty. */
                /* ทดสอบรหัส 0-9 ทีละรหัส 2 วินาที (4-pin BCD ครบทุกเลขแล้ว) */
                s_display_test_ticks = 200UL;
                (void)DrvUart_Write("DISPLAY TEST: observe physical digit and compare CODE below. Energy paused.\r\n");
                break;
            case (uint8_t)'h':
                (void)DrvUart_Write("CHARGE: Brightness <=900 gives zero solar. At 4095: 3000 units/s, 20s/level with loads OFF.\r\n");
                (void)DrvUart_Write("Battery rises only if solar > load and SOC <100%. At 100%, FULL is normal.\r\n");
                (void)DrvUart_Write("Use a/l/f to discharge first; then x to stop loads, shine light. Use i if brightness falls in light.\r\n");
                break;
            case (uint8_t)'1':
            case (uint8_t)'a': HandleButtons(BUTTON_AC_MASK); break;
            case (uint8_t)'2':
            case (uint8_t)'l': HandleButtons(BUTTON_LIGHT_MASK); break;
            case (uint8_t)'3':
            case (uint8_t)'f': HandleButtons(BUTTON_FAN_MASK); break;
            case (uint8_t)'4':
            case (uint8_t)'m': HandleButtons(BUTTON_MODE_MASK); break;
            case (uint8_t)'i': s_invert_light = !s_invert_light; s_force_telemetry = true; break;
            case (uint8_t)'0':
            case (uint8_t)'x':
                s_state.ac_auto_mode = false;
                s_state.ac_manual_request = false;
                s_state.fan_request = false;
                s_state.light_mode = LIGHT_MODE_MANUAL_OFF;
                s_force_telemetry = true;
                break;
            default: break;
        }
    }

    while (ticks > 0UL)
    {
        const AdcSamples_t adc = DrvAdcDma_GetLatest();
        const uint16_t brightness = NormalizeBrightness(adc.light_raw);

        /* ทดสอบ output สลับติด/ดับทุก 0.5 วินาที ก่อนเริ่มจำลองพลังงาน */
        if (s_startup_ticks < 20UL)
        {
            const bool test_on = ((s_startup_ticks / 5UL) % 2UL) == 0UL;
            DrvGpio_SetLed(DEVICE_LED_AC, test_on);
            DrvGpio_SetLed(DEVICE_LED_LIGHT, test_on);
            DrvGpio_SetLed(DEVICE_LED_FAN, test_on);
            DrvGpio_SetLed(DEVICE_LED_ECO, test_on);
            DrvSevenSeg_ShowBattery(9U, DrvTimer_GetMs());
            s_startup_ticks++;
            ticks--;
            continue;
        }

        if (s_display_test_ticks > 0UL)
        {
            const uint32_t code = (200UL - s_display_test_ticks) / 20UL;
            DrvGpio_SetSevenSegCode((uint8_t)code);
            if ((s_display_test_ticks % 20UL) == 0UL)
            {
                (void)DrvUart_Write("DISPLAY CODE=");
                (void)DrvUart_WriteU32(code);
                (void)DrvUart_Write(" [PC7,PA8,PB10,PA9]=");
                (void)DrvUart_WriteU32(code & 1UL);
                (void)DrvUart_WriteU32((code >> 1UL) & 1UL);
                (void)DrvUart_WriteU32((code >> 2UL) & 1UL);
                (void)DrvUart_WriteU32((code >> 3UL) & 1UL);
                (void)DrvUart_Write("\r\n");
            }
            s_display_test_ticks--;
            if (s_display_test_ticks == 0UL)
            {
                ApplyIndicators();
                s_force_telemetry = true;
                (void)DrvUart_Write("DISPLAY TEST END; simulation resumed\r\n");
            }
            ticks--;
            continue;
        }

        s_state.temperature_valid = (adc.sequence != 0UL) && AppTemperature_Convert(adc.temperature_raw, &s_state.temperature_tenths_c);
        s_state.target_c = (uint8_t)(18UL + (((uint32_t)adc.potentiometer_raw * 12UL) / ADC_FULL_SCALE));
        EvaluateOutputs(brightness);
        UpdateEnergy(brightness);
        /* ถ้าระดับเพิ่งต่ำกว่า 4 ใน tick นี้ ให้ปิดโหลดหนักภายใน event เดียวกัน */
        EvaluateOutputs(brightness);
        ApplyIndicators();

        s_telemetry_ticks++;
        if ((s_telemetry_ticks >= (1000UL / APP_TICK_MS)) || s_force_telemetry)
        {
            s_telemetry_ticks = 0UL;
            s_force_telemetry = false;
            PrintTelemetry(&adc);
        }
        ticks--;
    }

}
