/**
 * @file app_temperature.c
 * @brief แปลง ADC เป็นอุณหภูมิหน่วย 0.1°C; ค่า NTC เริ่มต้นเป็นประมาณการ ไม่ใช่ผล calibration
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "app_temperature.h"
#include "project_config.h"
#include <math.h>

/**
 * คืนชื่อ model ที่เลือกตอน compile; ESTIMATE เป็น label ไม่ได้ปิดการควบคุม Auto
 */
const char *AppTemperature_Model(void)
{
#if TEMP_SENSOR_MODEL == TEMP_MODEL_LM35
    return "LM35";
#elif TEMP_SENSOR_MODEL == TEMP_MODEL_TMP36
    return "TMP36";
#elif TEMP_SENSOR_MODEL == TEMP_MODEL_NTC
#if NTC_PARAMETERS_VERIFIED == 1U
    return "NTC";
#else
    return "NTC ESTIMATE (R25=10k B=3950 assumed)";
#endif
#else
    return "UNCONFIGURED";
#endif
}

/**
 * คืน true เมื่อแปลง raw สำเร็จและเขียน tenths_c หน่วย 0.1°C; false เมื่อผิดช่วง/null
 * NTC: 1/T = 1/298.15 + ln(R/R25)/Beta; LM35/TMP36 แปลงตาม voltage model
 * มี floating point ในฟังก์ชันนี้ แม้ state ที่ application เก็บเป็น integer
 */
bool AppTemperature_Convert(uint16_t raw, int16_t *tenths_c)
{
    /* ADC ใกล้ rail อาจเป็นสายขาด/ลัดวงจร ไม่ส่งต่อเป็นอุณหภูมิจริง */
    if ((raw < 5U) || (raw > 4090U) || (tenths_c == 0)) { return false; }
#if TEMP_SENSOR_MODEL == TEMP_MODEL_UNKNOWN
    return false;
#else
    float c;
#if TEMP_SENSOR_MODEL == TEMP_MODEL_LM35
    c = ((float)raw * (float)ADC_REFERENCE_MV) / ((float)ADC_FULL_SCALE * 10.0f);
    if ((c < 0.0f) || (c > 150.0f)) { return false; }
#elif TEMP_SENSOR_MODEL == TEMP_MODEL_TMP36
    c = ((((float)raw * (float)ADC_REFERENCE_MV) / (float)ADC_FULL_SCALE) - 500.0f) / 10.0f;
    if ((c < -40.0f) || (c > 125.0f)) { return false; }
#elif TEMP_SENSOR_MODEL == TEMP_MODEL_NTC
#if NTC_TO_GROUND == 1U
    const float resistance = NTC_FIXED_OHMS * (float)raw / ((float)ADC_FULL_SCALE - (float)raw);
#else
    const float resistance = NTC_FIXED_OHMS * ((float)ADC_FULL_SCALE - (float)raw) / (float)raw;
#endif
    if ((NTC_BETA_K <= 0.0f) || (NTC_R25_OHMS <= 0.0f) || (resistance <= 0.0f)) { return false; }
    const float inverse_k = (1.0f / 298.15f) + logf(resistance / NTC_R25_OHMS) / NTC_BETA_K;
    if (inverse_k <= 0.0f) { return false; }
    c = (1.0f / inverse_k) - 273.15f;
    if (!isfinite(c) || (c < -40.0f) || (c > 125.0f)) { return false; }
#else
#error "Unsupported temperature model"
#endif
    *tenths_c = (int16_t)((c * 10.0f) + ((c >= 0.0f) ? 0.5f : -0.5f));
    return true;
#endif
}
