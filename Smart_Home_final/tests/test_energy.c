/* ทดสอบ source จริง โดยจำลองเฉพาะขอบเขตฮาร์ดแวร์ ไม่แฟลชบอร์ด */
#include "../Application/Src/app_energy.c"

static uint32_t test_ticks;
static uint32_t test_buttons;
static uint8_t test_key;
static uint32_t test_eco_events;
static uint8_t test_digit;
static bool test_leds[4];
static AdcSamples_t test_adc;
volatile uint32_t test_checks;

bool DrvUart_Write(const char *text)
{
    if ((text[0] == '[') && (text[1] == 'E') && (text[2] == 'C')) { ++test_eco_events; }
    return true;
}
bool DrvUart_WriteU32(uint32_t value) { (void)value; return true; }
bool DrvUart_ReadByte(uint8_t *value)
{
    if (test_key == 0U) { return false; }
    *value = test_key; test_key = 0U; return true;
}
uint32_t DrvTimer_GetMs(void) { return 3000U; }
uint32_t DrvTimer_TakeTicks(void) { uint32_t value = test_ticks; test_ticks = 0U; return value; }
uint32_t DrvButtonsExti_TakeEvents(void) { uint32_t value = test_buttons; test_buttons = 0U; return value; }
AdcSamples_t DrvAdcDma_GetLatest(void) { return test_adc; }
bool AppTemperature_Convert(uint16_t raw, int16_t *value) { (void)raw; *value = 350; return true; }
const char *AppTemperature_Model(void) { return "TEST"; }
void DrvGpio_SetLed(DeviceLed_t led, bool on) { test_leds[led] = on; }
void DrvGpio_SetSevenSegCode(uint8_t value) { test_digit = value; }
uint8_t DrvGpio_GetSevenSegPins(void) { return test_digit; }
void DrvSevenSeg_ShowBattery(uint8_t value, uint32_t now) { (void)now; test_digit = value; }

static void ResetTest(uint32_t energy)
{
    s_state = (EnergyState_t){0};
    s_state.energy = energy;
    s_state.battery_level = CalculateBatteryLevel(energy);
    s_state.target_c = 24U;
    s_state.temperature_tenths_c = 350;
    s_state.temperature_valid = true;
    s_state.light_mode = LIGHT_MODE_MANUAL_OFF;
    s_startup_ticks = 20U;
    s_display_test_ticks = 0U;
    s_telemetry_ticks = 0U;
    s_force_telemetry = false;
    s_invert_light = false;
    test_ticks = 0U; test_buttons = 0U; test_key = 0U; test_eco_events = 0U;
    test_adc = (AdcSamples_t){2000U, 4095U, 2048U, 1U};
}
#define CHECK(condition) do { ++test_checks; if (!(condition)) { return __LINE__; } } while (0)

int test_main(void)
{
    ResetTest(120000U);
    CHECK(NormalizeBrightness(4095U) == 0U);
    CHECK(NormalizeBrightness(0U) == 4095U);
    test_key = 'A'; AppEnergy_RunOnce();
    CHECK(s_state.power_saving && s_state.ac_on && test_leds[DEVICE_LED_AC]);
    test_buttons = BUTTON_FAN_MASK; AppEnergy_RunOnce();
    CHECK(s_state.fan_on && test_leds[DEVICE_LED_FAN]);
    test_ticks = 1U; AppEnergy_RunOnce();
    CHECK(s_state.ac_on && s_state.fan_on && s_state.cost_per_tick > 150U);
    /* Normal -> Eco หลังหักค่าโหลด: ตัดใน tick เดียวกัน */
    ResetTest(180001U);
    s_state.ac_manual_request = true; s_state.fan_request = true;
    test_ticks = 1U;
    AppEnergy_RunOnce();
    CHECK(s_state.battery_level == 3U && s_state.power_saving);
    CHECK(!s_state.ac_on && !s_state.fan_on && test_eco_events == 1U);
    CHECK(!s_state.ac_manual_request && !s_state.fan_request);
    CHECK(test_digit == 3U && test_leds[DEVICE_LED_ECO]);

    /* กดปุ่มจริงเปิดทุกโหลดใน Eco: ต้องไม่ถูกปิดซ้ำ */
    uint32_t before = s_state.energy;
    test_buttons = BUTTON_AC_MASK | BUTTON_FAN_MASK | BUTTON_LIGHT_MASK;
    test_ticks = 1U;
    AppEnergy_RunOnce();
    CHECK(s_state.ac_on && s_state.fan_on && s_state.light_on);
    CHECK(s_state.cost_per_tick == (100U + 150U + 200U + (30U - s_state.target_c) * 8U));
    CHECK(s_state.energy == before - s_state.cost_per_tick);
    test_ticks = 3U; AppEnergy_RunOnce();
    CHECK(s_state.ac_on && s_state.fan_on && test_eco_events == 1U);

    /* ลดย่อย3->2->1ไม่ใช่การเข้าEcoใหม่ */
    s_state.energy = 100000U; s_state.battery_level = 2U; EvaluateOutputs(0U);
    CHECK(s_state.ac_on && s_state.fan_on);
    s_state.energy = 50000U; s_state.battery_level = 1U; EvaluateOutputs(0U);
    CHECK(s_state.ac_on && s_state.fan_on && test_eco_events == 1U);

    /* ออกจากEcoแล้วเข้าใหม่ ต้องบังคับปิดอีกรอบ */
    s_state.energy = 240000U; s_state.battery_level = 4U; EvaluateOutputs(0U);
    CHECK(!s_state.power_saving && s_state.ac_on);
    s_state.energy = 180000U; s_state.battery_level = 3U; EvaluateOutputs(0U);
    CHECK(!s_state.ac_on && !s_state.fan_on && s_state.light_on && test_eco_events == 2U);

    /* Auto ไม่เปิดเองหลังเข้าEco แต่ผู้ใช้เลือกMใหม่ได้ */
    ResetTest(240000U); s_state.ac_auto_mode = true; EvaluateOutputs(0U);
    CHECK(s_state.ac_on);
    s_state.energy = 180000U; s_state.battery_level = 3U; EvaluateOutputs(0U);
    CHECK(!s_state.ac_auto_mode && !s_state.ac_on);
    EvaluateOutputs(0U); CHECK(!s_state.ac_on && test_eco_events == 1U);
    test_key = 'm'; test_ticks = 1U; AppEnergy_RunOnce();
    CHECK(s_state.ac_auto_mode && s_state.ac_on);
    s_state.temperature_valid = false; EvaluateOutputs(0U); CHECK(!s_state.ac_on);

    /* UART A/F เปิดในEco และ X ปิดทุกโหลด */
    test_key = 'a'; test_ticks = 1U; AppEnergy_RunOnce();
    CHECK(s_state.ac_on && !s_state.ac_auto_mode);
    test_key = 'f'; test_ticks = 1U; AppEnergy_RunOnce(); CHECK(s_state.fan_on);
    test_key = 'x'; test_ticks = 1U; AppEnergy_RunOnce();
    CHECK(!s_state.ac_on && !s_state.fan_on && !s_state.light_on);

    /* ใช้แบตจนหมด: ปิดและบล็อกทุกโหลด แม้เพิ่งเปิดในEco */
    ResetTest(1U); EvaluateOutputs(0U);
    HandleButtons(BUTTON_AC_MASK | BUTTON_FAN_MASK | BUTTON_LIGHT_MASK);
    test_ticks = 1U; AppEnergy_RunOnce();
    CHECK(s_state.energy == 0U && !s_state.ac_on && !s_state.fan_on && !s_state.light_on);
    CHECK(test_digit == 0U && !test_leds[DEVICE_LED_ECO]);
    HandleButtons(BUTTON_AC_MASK | BUTTON_FAN_MASK | BUTTON_LIGHT_MASK | BUTTON_MODE_MASK);
    CHECK(!s_state.ac_manual_request && !s_state.fan_request && !s_state.ac_auto_mode);
    CHECK(s_state.light_mode == LIGHT_MODE_MANUAL_OFF);

    /* ชาร์จจากศูนย์ยังไม่เปิดเอง และเปิดใหม่ได้เมื่อสะสมถึง EMPTY_RESTART_ENERGY */
    test_adc.light_raw = 0U; test_ticks = 1U; AppEnergy_RunOnce();
    CHECK(s_state.energy == SOLAR_MAX_PER_TICK && s_state.battery_level == 0U);
    CHECK(!s_state.ac_on && !s_state.fan_on && !s_state.light_on);
    CHECK(!test_leds[DEVICE_LED_ECO] && test_digit == 0U);
    test_key = 'f'; test_ticks = 1U; AppEnergy_RunOnce();
    CHECK(!s_state.fan_on && s_state.empty_lock);
    test_ticks = EMPTY_RESTART_ENERGY / SOLAR_MAX_PER_TICK - 2U;
    AppEnergy_RunOnce();
    CHECK(!s_state.empty_lock && s_state.battery_level == 1U);
    CHECK(s_state.energy == EMPTY_RESTART_ENERGY);
    test_key = 'f'; test_ticks = 1U; AppEnergy_RunOnce();
    CHECK(s_state.fan_on && s_state.cost_per_tick == 150U);
    CHECK(s_state.energy == EMPTY_RESTART_ENERGY + SOLAR_MAX_PER_TICK - 150U);
    return 0;
}
