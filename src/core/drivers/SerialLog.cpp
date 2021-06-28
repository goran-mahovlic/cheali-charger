/*
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
#include "LcdPrint.h"
#include "Program.h"
#include "Settings.h"
#include "memory.h"
#include "Version.h"
#include "TheveninMethod.h"
#include "StackInfo.h"
#include "IO.h"
#include "SerialLog.h"
#include "AnalogInputsPrivate.h"
#include "Balancer.h"
#include "Time.h"
#include "Screen.h"

#ifdef ENABLE_SERIAL_LOG
#include "Serial.h"
#endif //ENABLE_SERIAL_LOG

#include "Monitor.h"

#define BB3
uint8_t small_delay = 100;
uint16_t big_delay = 500;
uint16_t Vtmp;
uint16_t v;

void LogDebug_run() __attribute__((weak));
void LogDebug_run()
{}

namespace SerialLog {
    enum State { On, Off, Starting };
    uint32_t startTime;
    uint32_t currentTime;

    State state = Off;
    uint8_t CRC;
    const AnalogInputs::Name channel1[] PROGMEM = {
            AnalogInputs::VoutBalancer,
            AnalogInputs::Iout,
            AnalogInputs::Cout,
            AnalogInputs::Pout,
            AnalogInputs::Eout,
            AnalogInputs::Textern,
            AnalogInputs::Tintern,
            AnalogInputs::Vin,
            AnalogInputs::Vb1,
            AnalogInputs::Vb2,
            AnalogInputs::Vb3,
            AnalogInputs::Vb4,
            AnalogInputs::Vb5,
            AnalogInputs::Vb6,
            BALANCER_PORTS_GT_6(AnalogInputs::Vb7,AnalogInputs::Vb8,)
    };

void sendTime();
void dlogInit();
void dlogDeInit();

#ifdef ENABLE_SERIAL_LOG

void serialBegin()
{
    Serial::begin(settings.getUARTspeed());
    dlogInit();
}
void serialEnd()
{
	dlogDeInit();
    Serial::flush();
    Serial::end();
}

void printChar(char c)
{
    Serial::write(c);
    CRC^=c;
}

void powerOn()
{
    if(state != Off)
        return;
    if(settings.UART == Settings::Disabled)
        return;

#ifdef ENABLE_EXT_TEMP_AND_UART_COMMON_OUTPUT
    if(ProgramData::battery.enable_externT)
        if(settings.UARToutput == Settings::TempOutput)
            return;
#endif

    serialBegin();

    state = Starting;
}

void powerOff()
{
    if(state == Off)
        return;

    serialEnd();
    state = Off;
}

void send()
{
    if(state == Off)
        return;

    currentTime = Time::getMiliseconds();

    if(state == Starting) {
        startTime = currentTime;
        state = On;
    }

    currentTime -= startTime;
    sendTime();
}

void flush()
{
    if(state == Off)
        return;
    Serial::flush();
}


void doIdle()
{
    static uint16_t analogCount;
    if(!AnalogInputs::isPowerOn()) {
        analogCount = 0;
    } else {
        uint16_t c = AnalogInputs::getFullMeasurementCount();
        if(analogCount != c) {
            analogCount = c;
            send();
        }
    }
    LogDebug_run();
}

#else //ENABLE_SERIAL_LOG

void serialBegin(){}
void serialEnd(){}

void printChar(char c){}
void powerOn(){}
void powerOff(){}
void send(){}
void doIdle(){}
void flush(){}


#endif


void printD()
{
    printChar(';');
}

void printString(const char *s)
{
    while(*s) {
        printChar(*s);
        s++;
    }
}

void printString_P(const char *s)
{
    char c;
    while(1) {
        c = pgm::read(s);
        if(!c)
            return;

        printChar(c);
        s++;
    }
}



void printLong(int32_t x)
{
    char buf[15];
    ::printLong(x, buf);
    printString(buf);
}



void sendHeader(uint16_t channel)
{
    CRC = 0;
    printChar('$');
    printUInt(channel);
    printD();
    printUInt(Program::programType+1);
    printD();

    printLong(currentTime/1000);   //timestamp
    printChar('.');
    printUInt((currentTime/100)%10);   //timestamp
    printD();
}


void printNL()
{
    printChar('\r');
    printChar('\n');
}

void sendEnd()
{
	#ifndef BB3
    //checksum
    	printUInt(CRC);
	#endif
    printNL();
}

void sendChannel1()
{



#ifndef BB3
    sendHeader(1);

    //analog inputs
    for(uint8_t i=0;i < sizeOfArray(channel1);i++) {
        AnalogInputs::Name name = pgm::read(&channel1[i]);
        v = AnalogInputs::getRealValue(name);
        printUInt(v);
        printD();
    }

    for(uint8_t i=0;i<MAX_BALANCE_CELLS;i++) {
        printUInt(TheveninMethod::getReadableRthCell(i));
        printD();
    }

    printUInt(TheveninMethod::getReadableBattRth());
    printD();

    printUInt(TheveninMethod::getReadableWiresRth());
    printD();

    printUInt(Monitor::getChargeProcent());
    printD();
    printLong(Monitor::getETATime());
    printD();

    sendEnd();
#else
    /*
    AnalogInputs::Name nameVoltage = pgm::read(&channel1[0]);
    v = AnalogInputs::getRealValue(nameVoltage);
    printString("SENS:DLOG:TRACE:DATA ");
    uint16_t Vtmp = v/1000;
    printUInt(Vtmp);
    printString(".");
    v = (v-(Vtmp*1000));
    printUInt(v);
    printString(", ");
    //5,6
    AnalogInputs::Name nameCurrent = pgm::read(&channel1[1]);
    uint16_t i = AnalogInputs::getRealValue(nameCurrent);
    printUInt(i);
    */
    //analog inputs

    printString("SENS:DLOG:TRACE:DATA ");

    for(uint8_t i=0;i < 8;i++) {
        AnalogInputs::Name name = pgm::read(&channel1[i]);
        v = AnalogInputs::getRealValue(name);
        if (i==0 || i==1 || i==2 || i==7){
            Vtmp = v/1000;
            printUInt(Vtmp);
            printString(".");
            v = (v-(Vtmp*1000));
        }
        else if (i==3 || i==4 || i==6){
            Vtmp = v/100;
            printUInt(Vtmp);
            printString(".");
            v = (v-(Vtmp*100));
        }
        else if(i==5){
        	v=0;
        }
        printUInt(v);
        printString(", ");
    }

//    for(uint8_t i=0;i<MAX_BALANCE_CELLS;i++) {
//        printUInt(TheveninMethod::getReadableRthCell(i));
//        printString(", ");
//    }
    v = TheveninMethod::getReadableBattRth();
    Vtmp = v/1000;
    printUInt(Vtmp);
    printString(".");
    v = (v-(Vtmp*1000));
    printUInt(v);
    printString(", ");

    v = TheveninMethod::getReadableWiresRth();
    Vtmp = v/1000;
    printUInt(Vtmp);
    printString(".");
    v = (v-(Vtmp*1000));
    printUInt(v);
    printString(", ");

    printUInt(Monitor::getChargeProcent());

    sendEnd();
    //printD();

#endif
}

void sendChannel2(bool adc)
{
#ifndef BB3
    sendHeader(2);
    ANALOG_INPUTS_FOR_ALL(it) {
        if(adc) v = AnalogInputs::getAvrADCValue(it);
        else    v = AnalogInputs::getRealValue(it);
        printUInt(v);
        printD();
    }
    printUInt(Balancer::balance);
    printD();

    uint16_t pidV=0;
#ifdef ENABLE_GET_PID_VALUE
    pidV = hardware::getPIDValue();
#endif
    printUInt(pidV);
    printD();
    sendEnd();
#endif
}

void sendChannel3()
{
#ifndef BB3
    sendHeader(3);
#ifdef    ENABLE_STACK_INFO //ENABLE_SERIAL_LOG
    printUInt(StackInfo::getNeverUsedStackSize());
    printD();
    printUInt(StackInfo::getFreeStackSize());
    printD();
#endif
    sendEnd();
#endif
}

void dlogInit(){
	//printString("DISP:TEXT:CLE\r\n");
	//printString("\r\n");
	Screen::displayStrings(PSTR("BB3 logging ON"));
	printString("DISP:TEXT:CLE\r\n");
	printString("DISP:TEXT 'Imax B6 in controll!'\r\n");
	Time::delay(big_delay);
	printString("DISP:TEXT:CLE\r\n");
	Time::delay(big_delay);
	printString("DISP:TEXT 'Data log will start in 10 sec...'\r\n");
	Time::delay(big_delay);
	Screen::displayStrings(PSTR("Setting time"));
	// Time X axis
	printString("SENS:DLOG:TRAC:X:UNIT SECO\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:X:STEP 1\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:X:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:X:RANG:MAX 20\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:X:LAB \"t\"\r\n");
	Time::delay(big_delay);
	// Vout 0
	Screen::displayStrings(PSTR("Setting Vout"));
	printString("SENS:DLOG:TRAC:Y1:UNIT VOLT\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y1:LAB \"U\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y1:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y1:RANG:MAX 12\r\n");
	Time::delay(big_delay);
	// Iout 1
	Screen::displayStrings(PSTR("Setting Iout"));
	printString("SENS:DLOG:TRAC:Y2:UNIT AMPE\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y2:LAB \"I\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y2:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y2:RANG:MAX 5\r\n");
	Time::delay(big_delay);
	// Cout 2
	Screen::displayStrings(PSTR("Setting Cout"));
	//printString("SENS:DLOG:TRAC:Y3:UNIT Ah\r\n");
	//Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y3:LAB \"C\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y3:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y3:RANG:MAX 50\r\n");
	Time::delay(big_delay);
	//Pout 3
	Screen::displayStrings(PSTR("Setting Pout"));
	printString("SENS:DLOG:TRAC:Y4:UNIT WATT\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y4:LAB \"P\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y4:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y4:RANG:MAX 50\r\n");
	Time::delay(big_delay);
	// Eout 4
	Screen::displayStrings(PSTR("Setting Eout"));
	//printString("SENS:DLOG:TRAC:Y5:UNIT Ws\r\n");
	//Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y5:LAB \"E\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y5:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y5:RANG:MAX 50\r\n");
	Time::delay(big_delay);
	// Temperature external 5
	Screen::displayStrings(PSTR("Setting T_ext"));
	//printString("SENS:DLOG:TRAC:Y6:UNIT C\r\n");
	printString("SENS:DLOG:TRAC:Y6:LAB \"Text\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y6:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y6:RANG:MAX 200\r\n");
	Time::delay(big_delay);
	// Temperature internal 6
	Screen::displayStrings(PSTR("Setting T_int"));
	//printString("SENS:DLOG:TRAC:Y7:UNIT C\r\n");
	printString("SENS:DLOG:TRAC:Y7:LAB \"Tint\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y7:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y7:RANG:MAX 200\r\n");
	Time::delay(big_delay);
	// Vin 7
	Screen::displayStrings(PSTR("Setting Vin"));
	printString("SENS:DLOG:TRAC:Y8:UNIT VOLT\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y8:LAB \"Uin\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y8:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y8:RANG:MAX 20\r\n");
	Time::delay(big_delay);
	// Rbat 8
	Screen::displayStrings(PSTR("Setting Rbat"));
	printString("SENS:DLOG:TRAC:Y9:UNIT OHM\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y9:LAB \"Rbat\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y9:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y9:RANG:MAX 10\r\n");
	Time::delay(big_delay);
	// Rwire 9
	Screen::displayStrings(PSTR("Setting Rwire"));
	printString("SENS:DLOG:TRAC:Y10:UNIT OHM\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y10:LAB \"Rwire\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y10:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y10:RANG:MAX 10\r\n");
	Time::delay(big_delay);
	// Charge percent 10
	Screen::displayStrings(PSTR("Setting charge %"));
//	printString("SENS:DLOG:TRAC:Y11:UNIT %\r\n");
	printString("SENS:DLOG:TRAC:Y11:LAB \"Charge\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y11:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y11:RANG:MAX 100\r\n");
	Time::delay(big_delay);

	/*
	// Vch1
	Screen::displayStrings(PSTR("Setting Vout"));
	printString("SENS:DLOG:TRAC:Y12:UNIT VOLT\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y12:LAB \"Uch1\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y12:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y12:RANG:MAX 40000\r\n");
	Time::delay(big_delay);
	// Ich1
	Screen::displayStrings(PSTR("Setting Iout"));
	printString("SENS:DLOG:TRAC:Y13:UNIT AMPE\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y13:LAB \"Ich1\"\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y13:RANG:MIN 0\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y13:RANG:MAX 5000\r\n");
	Time::delay(big_delay);
	*/

	Screen::displayStrings(PSTR("Setting scale"));
	printString("SENS:DLOG:TRAC:X:SCAL LIN\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:Y:SCAL LIN\r\n");
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:REM \"Imax B6 remark\"\r\n");
	Time::delay(small_delay);
	Screen::displayStrings(PSTR("Init DLOG"));
	printString("INIT:DLOG:TRACE \"/Recordings/imaxB6.dlog\"\r\n");
	Time::delay(big_delay);
	printString("SENS:DLOG:TRAC:BOOK \"Start\"\r\n");
	Time::delay(small_delay);
	printString("DISP:TEXT:CLE\r\n");
	Time::delay(small_delay);
}

void dlogDeInit(){
	Screen::displayStrings(PSTR("BB3 logging OFF"));
	Time::delay(small_delay);
	printString("SENS:DLOG:TRAC:BOOK \"Stop\"\r\n");
	Time::delay(big_delay);
	printString("ABOR:DLOG\r\n");
	Time::delay(big_delay);
	printString("DISP:TEXT \"Imax B6 checking out!\"\r\n");
	Time::delay(big_delay);
	printString("DISP:TEXT:CLE\r\n");
	Time::delay(small_delay);
}

void sendTime()
{
    int uart = settings.UART;
    bool adc = false;

    STATIC_ASSERT(Settings::ExtDebugAdc == 4);

    if(uart > Settings::ExtDebug) {
        adc = true;
    }
    sendChannel1();
    if(uart > Settings::Normal)
        sendChannel2(adc);

    if(uart > Settings::Debug)
        sendChannel3();
}

} //namespace SerialLog
