/**
 * @file drv_adc_dma.c
 * @brief ADC1 scan PA0/PA1/PA4: Timer เริ่ม conversion และ DMA2 Stream0 ส่ง 3 samples ลง RAM โดยไม่รอ EOC ใน main
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "drv_adc_dma.h"
#include "stm32f411xe.h"

static volatile uint16_t s_dma_samples[3];
static volatile AdcSamples_t s_latest;

/**
 * ตั้งลำดับ ADC channel 0,1,4 และ DMA circular buffer 3 ค่า ขนาด halfword
 * ผลแต่ละ index ต้องตรง temperature/light/potentiometer ใน IRQ handler
 */
void DrvAdcDma_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;

    ADC->CCR = (ADC->CCR & ~ADC_CCR_ADCPRE) | ADC_CCR_ADCPRE_0; /* PCLK2/4 */

    ADC1->CR1 = ADC_CR1_SCAN;
    ADC1->CR2 = ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_EOCS;
    ADC1->SMPR2 = (7UL << ADC_SMPR2_SMP0_Pos) |
                  (7UL << ADC_SMPR2_SMP1_Pos) |
                  (7UL << ADC_SMPR2_SMP4_Pos);
    ADC1->SQR1 = (2UL << ADC_SQR1_L_Pos); /* 3 conversions */
    ADC1->SQR3 = (0UL << ADC_SQR3_SQ1_Pos) |
                 (1UL << ADC_SQR3_SQ2_Pos) |
                 (4UL << ADC_SQR3_SQ3_Pos);

    DMA2_Stream0->CR = 0UL;
    while ((DMA2_Stream0->CR & DMA_SxCR_EN) != 0UL)
    {
        /* ต้องยืนยันว่า stream หยุดก่อนเขียน configuration */
    }
    DMA2->LIFCR = DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CTEIF0 |
                  DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTCIF0;
    DMA2_Stream0->PAR = (uint32_t)(uintptr_t)&ADC1->DR;
    DMA2_Stream0->M0AR = (uint32_t)(uintptr_t)&s_dma_samples[0];
    DMA2_Stream0->NDTR = 3UL;
    DMA2_Stream0->CR = DMA_SxCR_PL_1 | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0 |
                       DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_TCIE;

    NVIC_SetPriority(DMA2_Stream0_IRQn, 3UL);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    DMA2_Stream0->CR |= DMA_SxCR_EN;
    ADC1->CR2 |= ADC_CR2_ADON;
}

/**
 * เรียกจาก TIM2 ISR ทุก tick: เริ่ม scan ใหม่แล้วคืนทันที ไม่วนรอ conversion
 */
void DrvAdcDma_StartFromIsr(void)
{
    /* Timer ISR เป็นผู้เริ่ม sequence; main ไม่เคยวนรอ ADC */
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

/**
 * คืน snapshot รอบล่าสุดที่ DMA ทำเสร็จ ปิด IRQ สั้น ๆ เพื่อไม่ให้ field ถูกแก้ระหว่าง copy
 * sequence=0 หมายถึงยังไม่มีชุดข้อมูล; ไม่รับรองว่าข้อมูลใหม่ใน tick ปัจจุบัน
 */
AdcSamples_t DrvAdcDma_GetLatest(void)
{
    AdcSamples_t copy;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    copy = s_latest;
    __set_PRIMASK(primask);
    return copy;
}

/**
 * เมื่อ DMA ครบ 3 ค่า copy ไป snapshot และเพิ่ม sequence; error flags ถูกล้างแต่ยังไม่มี recovery/counter
 */
void DrvAdcDma_IrqHandler(void)
{
    if ((DMA2->LISR & DMA_LISR_TCIF0) != 0UL)
    {
        DMA2->LIFCR = DMA_LIFCR_CTCIF0;
        s_latest.temperature_raw = s_dma_samples[0];
        s_latest.light_raw = s_dma_samples[1];
        s_latest.potentiometer_raw = s_dma_samples[2];
        s_latest.sequence++;
    }

    if ((DMA2->LISR & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)) != 0UL)
    {
        DMA2->LIFCR = DMA_LIFCR_CTEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0;
    }
}
