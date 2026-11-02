/**
 * @file drv_gpio.c
 * @brief GPIO: ตั้งขา sensor/button/output และขับ LED active-high กับ BCD 4 บิต; ไม่ได้ขับเครื่องใช้ไฟฟ้าจริง
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "drv_gpio.h"
#include "stm32f411xe.h"

/**
 * ตั้งขา push-pull output ไม่มี pull-up/down; pin ต้องเป็นหมายเลขขาที่ valid ของ port
 */
static void SetOutput(GPIO_TypeDef *port, uint32_t pin)
{
    port->MODER = (port->MODER & ~(3UL << (pin * 2UL))) | (1UL << (pin * 2UL));
    port->OTYPER &= ~(1UL << pin);
    port->OSPEEDR = (port->OSPEEDR & ~(3UL << (pin * 2UL))) | (1UL << (pin * 2UL));
    port->PUPDR &= ~(3UL << (pin * 2UL));
}

/**
 * ปุ่มปกติเป็น HIGH และกดลง GND; ใช้ร่วมกับ falling-edge EXTI
 */
static void SetInputPullUp(GPIO_TypeDef *port, uint32_t pin)
{
    port->MODER &= ~(3UL << (pin * 2UL));
    port->PUPDR = (port->PUPDR & ~(3UL << (pin * 2UL))) | (1UL << (pin * 2UL));
}

/**
 * ปิด digital input path โดยเลือก analog mode สำหรับ ADC
 */
static void SetAnalog(GPIO_TypeDef *port, uint32_t pin)
{
    port->MODER |= (3UL << (pin * 2UL));
    port->PUPDR &= ~(3UL << (pin * 2UL));
}

/**
 * เปิด clock port A/B/C แล้วตั้ง PA0/1/4 analog, ปุ่ม pull-up และ LED/BCD output
 * BCD bit 8 ของ R3 ใช้ PA9; ตรวจสายจริงบน shield ก่อนเปลี่ยน mapping
 */
void DrvGpio_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;

    SetAnalog(GPIOA, 0UL);
    SetAnalog(GPIOA, 1UL);
    SetAnalog(GPIOA, 4UL);

    SetOutput(GPIOA, 5UL);
    SetOutput(GPIOA, 6UL);
    SetOutput(GPIOA, 7UL);
    SetOutput(GPIOA, 8UL);
    SetOutput(GPIOC, 7UL);
    SetOutput(GPIOB, 6UL);
    SetOutput(GPIOA, 9UL);
    SetOutput(GPIOB, 10UL);

    SetInputPullUp(GPIOA, 10UL);
    SetInputPullUp(GPIOB, 3UL);
    SetInputPullUp(GPIOB, 4UL);
    SetInputPullUp(GPIOB, 5UL);

    GPIOA->BSRR = (GPIO_BSRR_BR5 | GPIO_BSRR_BR6 | GPIO_BSRR_BR7 | GPIO_BSRR_BR8 | GPIO_BSRR_BR9);
    GPIOC->BSRR = GPIO_BSRR_BR7;
    GPIOB->BSRR = (GPIO_BSRR_BR6 | GPIO_BSRR_BR9 | GPIO_BSRR_BR10);
}

/**
 * แปลงชื่อโหลดเป็นขา: LIGHT=PA5, AC=PA6, FAN=PA7, ECO=PB6
 * BSRR ตั้ง/ล้างบิตโดยไม่ read-modify-write ODR
 */
void DrvGpio_SetLed(DeviceLed_t led, bool on)
{
    GPIO_TypeDef *port = GPIOA;
    uint32_t pin = 5UL;

    switch (led)
    {
        case DEVICE_LED_LIGHT: pin = 5UL; break;
        case DEVICE_LED_AC:    pin = 6UL; break;
        case DEVICE_LED_FAN:   pin = 7UL; break;
        case DEVICE_LED_ECO:   port = GPIOB; pin = 6UL; break;
        default: return;
    }

    port->BSRR = on ? (1UL << pin) : (1UL << (pin + 16UL));
}

/**
 * ส่งเลขฐานสิบเป็น BCD: น้ำหนัก 1=PC7, 2=PA8, 4=PB10, 8=PA9; clamp เกิน 9
 */
void DrvGpio_SetSevenSegCode(uint8_t code)
{
    /* BCD เต็ม 4 บิตรองรับ 0-9; ค่าที่เกิน 9 จะถูก clamp ไว้ที่ 9 เพื่อกันรหัสเพี้ยน */
    const uint32_t value = ((uint32_t)code > 9UL) ? 9UL : (uint32_t)code;

    /* NUCLEO-F411RE Arduino D8 is PA9 (UM1724); shield PB9 label conflicts. */
    SetOutput(GPIOA, 9UL);
    GPIOC->BSRR = ((value & 1UL) != 0UL) ? GPIO_BSRR_BS7 : GPIO_BSRR_BR7;
    GPIOA->BSRR = (((value & 2UL) != 0UL) ? GPIO_BSRR_BS8 : GPIO_BSRR_BR8) |
                  (((value & 8UL) != 0UL) ? GPIO_BSRR_BS9 : GPIO_BSRR_BR9);
    GPIOB->BSRR = ((value & 4UL) != 0UL) ? GPIO_BSRR_BS10 : GPIO_BSRR_BR10;
    __DSB();
}

/**
 * อ่านระดับขา MCU กลับเป็นรหัส BCD; ไม่ใช่การอ่านหรือยืนยันตัวเลขบนจอจริง
 */
uint8_t DrvGpio_GetSevenSegPins(void)
{
    /* อ่านระดับไฟฟ้าที่ MCU ไม่ใช่การอ่านรูปตัวเลขบน LED */
    return (uint8_t)(((GPIOC->IDR >> 7U) & 1U) |
                     (((GPIOA->IDR >> 8U) & 1U) << 1U) |
                     (((GPIOB->IDR >> 10U) & 1U) << 2U) |
                     (((GPIOA->IDR >> 9U) & 1U) << 3U));
}
