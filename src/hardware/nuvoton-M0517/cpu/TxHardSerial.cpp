/*
    TxHardSerial - Hardware serial library (transmit only)
    Copyright (c) 2014 Sasa Mihajlovic.  All right reserved.

    cheali-charger - open source firmware for a variety of LiPo chargers
    Copyright (C) 2013  Paweł Stawicki. All right reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <atomic>
#include "Hardware.h"
#include "TxHardSerial.h"
#include "irq_priority.h"
#include "Settings.h"
#include "Serial.h"
#include "Screen.h"

#include "IO.h"

# if defined ( __GNUC__ )
#define RXBUFSIZE 32
#else
#define RXBUFSIZE 1024
#endif

/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
uint8_t g_u8RecData[RXBUFSIZE]  = {0};

volatile uint32_t g_u32comRbytes = 0;
volatile uint32_t g_u32comRhead  = 0;
volatile uint32_t g_u32comRtail  = 0;
volatile int32_t g_bWait         = 1;

namespace TxHardSerial {

#define Tx_BUFFER_SIZE  256

uint8_t  *txBuffer_= Serial::txBuffer;

std::atomic<uint16_t> tail_(0);
std::atomic<uint16_t> head_(0);


void initialize()
{
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART_S_HXT, CLK_CLKDIV_UART(1));
}

void begin(unsigned long baud)
{
    if(settings.UARToutput == Settings::HardwarePin7) {
        SYS->P3_MFP = (SYS->P3_MFP & (~SYS_MFP_P31_Msk)) | SYS_MFP_P31_TXD0; //Tx on pin 7
    } else {
        SYS->P0_MFP = (SYS->P0_MFP & (~SYS_MFP_P02_Msk)) | SYS_MFP_P02_TXD0; //Tx on pin 38
    }
    /* Configure UART0 and set UART0 Baudrate */
    UART0->BAUD = UART_BAUD_MODE2 | UART_BAUD_MODE2_DIVIDER(__HXT, baud);
    UART0->LCR = UART_WORD_LEN_8 | UART_PARITY_NONE | UART_STOP_BIT_1;
    UART0->IER = UART_IER_THRE_IEN_Msk;

    NVIC_SetPriority(UART0_IRQn,HARDWARE_SERIAL_IRQ_PRIORITY);
}


void write(uint8_t ucData)
{
    uint16_t i = (head_.load(std::memory_order_relaxed) + 1) % Tx_BUFFER_SIZE;

    while(i == tail_.load(std::memory_order_acquire));

    txBuffer_[i] = ucData;
    head_.store(i,  std::memory_order_release);
    NVIC_EnableIRQ(UART0_IRQn);
}


void flush()
{
    while(tail_.load(std::memory_order_acquire) != head_.load(std::memory_order_relaxed));
    while((UART0->FSR & UART_FSR_TE_FLAG_Msk) == 0);
}

void end()
{
//    NVIC_DisableIRQ(UART0_IRQn);
    UART0->IER = 0;
}

extern "C"
{
void UART0_IRQHandler(void) {
    uint16_t i = tail_.load(std::memory_order_relaxed);
    NVIC_DisableIRQ(UART0_IRQn);
    uint8_t u8InChar = 0xFF;
    uint32_t u32IntSts = UART0->ISR;

    if(u32IntSts & UART_ISR_RDA_INT_Msk)
    {
    	NVIC_DisableIRQ(UART0_IRQn);
        /* Get all the input characters */
        while(UART_IS_RX_READY(UART0))
        {
            /* Get the character from UART Buffer */
            u8InChar = UART_READ(UART0);


            if(u8InChar == '0')
            {
                g_bWait = FALSE;
            }

            else if(u8InChar == 'a'){
            	Screen::displayMonitorError();
            }

            /* Check if buffer full */
            if(g_u32comRbytes < RXBUFSIZE)
            {
                /* Enqueue the character */
                g_u8RecData[g_u32comRtail] = u8InChar;
                g_u32comRtail = (g_u32comRtail == (RXBUFSIZE - 1)) ? 0 : (g_u32comRtail + 1);
                g_u32comRbytes++;
            }
        }

        NVIC_EnableIRQ(UART0_IRQn);
    }

/*
    if(i == head_.load(std::memory_order_acquire)) {
        if((UART0->FSR & UART_FSR_TE_FLAG_Msk) == 0)
           NVIC_DisableIRQ(UART0_IRQn);
        return;
    }
*/
/*
    while((UART0->FSR & UART_FSR_TX_FULL_Msk) == 0) {
        i = (i + 1) % Tx_BUFFER_SIZE;
        UART_WRITE(UART0, txBuffer_[i]);
        tail_.store(i, std::memory_order_release);
        if(i == head_.load(std::memory_order_acquire)) {
            break;
        }
    }
    */
}
}

} // namespace TxHardSerial

