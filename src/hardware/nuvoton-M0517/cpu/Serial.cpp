/*
    Serial - wrapper for Hardware OR Software serial library (transmit only)
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

#include "Hardware.h"
#include "Settings.h"
#include <stdio.h>
#include "M051Series.h"
#include "Screen.h"
#include "sys.h"

#ifdef ENABLE_TX_HW_SERIAL_PIN7_PIN38
#include "TxHardSerial.h"
#endif

#include "TxSoftSerial.h"

namespace Serial {
void empty(){}
void emptyUint8(uint8_t c){}

void UART_TEST_HANDLE(void);

void (*write)(uint8_t c) = emptyUint8;
void (*flush)() = empty;
void (*end)() = empty;

#define Tx_BUFFER_SIZE  256
uint8_t  txBuffer[Tx_BUFFER_SIZE];

void  begin(unsigned long baud)
{
#ifdef ENABLE_TX_HW_SERIAL_PIN7_PIN38
    if(settings.UARToutput == Settings::HardwarePin7 || settings.UARToutput == Settings::HardwarePin38) {
        write = &(TxHardSerial::write);
        flush = &(TxHardSerial::flush);
        end = &(TxHardSerial::end);
        TxHardSerial::begin(baud);
    } else {
        write = &(TxSoftSerial::write);
        flush = &(TxSoftSerial::flush);
        end = &(TxSoftSerial::end);
        TxSoftSerial::begin(baud);
    }
#else
    write = &(TxSoftSerial::write);
    flush = &(TxSoftSerial::flush);
    end = &(TxSoftSerial::end);
    TxSoftSerial::begin(baud);
#endif
};


void  initialize() {
#ifdef ENABLE_TX_HW_SERIAL_PIN7_PIN38
    TxHardSerial::initialize();
#endif

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init UART                                                                                               */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Reset UART0 */
    SYS_ResetModule(UART0_RST);

    /* Configure UART0 and set UART0 Baudrate */
    UART_Open(UART0, 57600);
    /* Enable Interrupt and install the call back function */
    UART_ENABLE_INT(UART0, (UART_IER_RDA_IEN_Msk | UART_IER_RTO_IEN_Msk));
    NVIC_EnableIRQ(UART0_IRQn);

    TxSoftSerial::initialize();
}

} // namespace Serial
