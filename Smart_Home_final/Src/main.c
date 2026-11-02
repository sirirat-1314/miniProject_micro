/**
 * @file main.c
 * @brief จุดเริ่มโปรแกรม: เริ่ม driver ตามลำดับ แล้วประมวลผลเหตุการณ์ใน main loop และพักด้วย WFI
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "app_energy.h"
#include "drv_adc_dma.h"
#include "drv_buttons_exti.h"
#include "drv_clock.h"
#include "drv_gpio.h"

#include "drv_timer.h"
#include "drv_uart_irq.h"
#include "stm32f411xe.h"

/**
 * เริ่ม clock ก่อน peripherals; วนงานใน main และตื่นจาก WFI เมื่อมี IRQ ไม่ใช้ RTOS
 */
int main(void)
{
    DrvClock_Init();
    DrvGpio_Init();
    DrvUart_Init();
    DrvAdcDma_Init();
    DrvButtonsExti_Init();
    DrvTimer_Init();

    AppEnergy_Init();

    for (;;)
    {
        AppEnergy_RunOnce();
        __WFI(); /* ตื่นเมื่อ Timer, DMA, EXTI หรือ UART interrupt เกิดขึ้น */
    }
}
