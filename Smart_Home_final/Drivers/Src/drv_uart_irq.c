/**
 * @file drv_uart_irq.c
 * @brief USART2 PA2/PA3: รับ RXNE และส่ง TXE ผ่าน interrupt; main ติดต่อผ่าน ring buffer ใน RAM
 * ฐานพฤติกรรม: Smart Home 1.4 R3; ชุดจัดระเบียบ Smart_Home_final
 */

#include "drv_uart_irq.h"
#include "project_config.h"
#include "stm32f411xe.h"

#define RX_BUFFER_SIZE (32U)

static volatile uint8_t s_tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint16_t s_tx_head;
static volatile uint16_t s_tx_tail;
static volatile uint8_t s_rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;

/**
 * เลื่อน TX index แบบวงแหวน; เว้นหนึ่งช่องเพื่อแยก buffer เต็มกับว่าง
 */
static uint16_t NextTx(uint16_t index)
{
    const uint16_t next = (uint16_t)(index + 1U);
    return (next >= UART_TX_BUFFER_SIZE) ? 0U : next;
}

/**
 * เลื่อน RX index แบบวงแหวน; RX 32 ช่องเก็บค้างได้จริงสูงสุด 31 bytes
 */
static uint8_t NextRx(uint8_t index)
{
    const uint8_t next = (uint8_t)(index + 1U);
    return (next >= RX_BUFFER_SIZE) ? 0U : next;
}

/**
 * ตั้ง PA2=TX, PA3=RX, AF7 และ USART2 115200 8N1 ตาม config
 * เปิด RX interrupt ตั้งแต่เริ่ม; TX interrupt เปิดเมื่อมีข้อมูลให้ส่ง
 */
void DrvUart_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;

    GPIOA->MODER = (GPIOA->MODER & ~((3UL << 4) | (3UL << 6))) |
                   (2UL << 4) | (2UL << 6);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~((15UL << 8) | (15UL << 12))) |
                    (7UL << 8) | (7UL << 12);
    GPIOA->OSPEEDR |= (2UL << 4) | (2UL << 6);

    USART2->BRR = (CPU_CLOCK_HZ + (UART_BAUD_RATE / 2UL)) / UART_BAUD_RATE;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    USART2->CR2 = 0UL;
    USART2->CR3 = 0UL;

    NVIC_SetPriority(USART2_IRQn, 5UL);
    NVIC_EnableIRQ(USART2_IRQn);
    USART2->CR1 |= USART_CR1_UE;
}

/**
 * คัดลอกข้อความลง TX queue ไม่รอ hardware ส่งจบ; false เมื่อ null หรือพื้นที่ไม่พอ
 * การสำรวจคิวอยู่ใน critical section; application ปัจจุบันทิ้งผลคืน จึงอาจเสียข้อความเมื่อเต็ม
 */
bool DrvUart_Write(const char *text)
{
    uint16_t required = 0U;
    uint16_t cursor;
    uint16_t free_count = 0U;
    uint32_t primask;

    if (text == (const char *)0)
    {
        return false;
    }
    while ((text[required] != '\0') && (required < (UART_TX_BUFFER_SIZE - 1U)))
    {
        required++;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    cursor = s_tx_head;
    while (NextTx(cursor) != s_tx_tail)
    {
        free_count++;
        cursor = NextTx(cursor);
    }
    if (required > free_count)
    {
        __set_PRIMASK(primask);
        return false;
    }

    for (cursor = 0U; cursor < required; cursor++)
    {
        s_tx_buffer[s_tx_head] = (uint8_t)text[cursor];
        s_tx_head = NextTx(s_tx_head);
    }
    USART2->CR1 |= USART_CR1_TXEIE;
    __set_PRIMASK(primask);
    return true;
}

/**
 * แปลง unsigned 32-bit เป็น decimal สูงสุด 10 หลักและ null terminator แล้วส่งเข้าคิว
 */
bool DrvUart_WriteU32(uint32_t value)
{
    char digits[11];
    char output[11];
    uint8_t count = 0U;
    uint8_t index;
    uint32_t working = value;

    do
    {
        digits[count] = (char)('0' + (working % 10UL));
        working /= 10UL;
        count++;
    } while ((working != 0UL) && (count < 10U));

    for (index = 0U; index < count; index++)
    {
        output[index] = digits[(uint8_t)(count - index - 1U)];
    }
    output[count] = '\0';
    return DrvUart_Write(output);
}

/**
 * อ่าน byte ที่ ISR รับไว้ใน RAM; false เมื่อคิวว่าง ไม่ใช่ polling รอ USART register
 */
bool DrvUart_ReadByte(uint8_t *value)
{
    bool available = false;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if ((value != (uint8_t *)0) && (s_rx_head != s_rx_tail))
    {
        *value = s_rx_buffer[s_rx_tail];
        s_rx_tail = NextRx(s_rx_tail);
        available = true;
    }
    __set_PRIMASK(primask);
    return available;
}

/**
 * RXNE/ORE: อ่าน DR และเก็บ byte ถ้าคิวไม่เต็ม; TXE: ส่งหนึ่ง byte แล้วปิด TXEIE เมื่อคิวว่าง
 */
void DrvUart_IrqHandler(void)
{
    const uint32_t status = USART2->SR;

    if ((status & (USART_SR_RXNE | USART_SR_ORE)) != 0UL)
    {
        const uint8_t byte = (uint8_t)USART2->DR;
        const uint8_t next = NextRx(s_rx_head);
        if (next != s_rx_tail)
        {
            s_rx_buffer[s_rx_head] = byte;
            s_rx_head = next;
        }
    }

    if (((status & USART_SR_TXE) != 0UL) && ((USART2->CR1 & USART_CR1_TXEIE) != 0UL))
    {
        if (s_tx_tail != s_tx_head)
        {
            USART2->DR = s_tx_buffer[s_tx_tail];
            s_tx_tail = NextTx(s_tx_tail);
        }
        else
        {
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}
