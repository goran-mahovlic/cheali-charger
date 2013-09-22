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
#include "Program.h"
#include "Hardware.h"
#include "ProgramData.h"
#include "LcdPrint.h"
#include "Utils.h"
#include "Screen.h"
#include "SimpleChargeStrategy.h"
#include "TheveninChargeStrategy.h"
#include "TheveninDischargeStrategy.h"
#include "ChargeBalanceStrategy.h"
#include "DeltaChargeStrategy.h"
#include "StorageStrategy.h"
#include "Monitor.h"
#include "memory.h"
#include "StartInfoStrategy.h"
#include "Buzzer.h"
#include "StaticMenu.h"
#include "Settings.h"



AnalogInputs::Name Program::iName_;
Program::ProgramType Program::programType_;
Program::ProgramState Program::programState_ = Program::Done;
const char * Program::stopReason_;

namespace {
    const Screen::ScreenType deltaChargeScreens[] PROGMEM = {
      Screen::ScreenFirst,
      Screen::ScreenDeltaFirst,
      Screen::ScreenDeltaVout,
      Screen::ScreenDeltaTextern,
      Screen::ScreenR,
      Screen::ScreenVout,
      Screen::ScreenVinput,
      Screen::ScreenTime,
      Screen::ScreenTemperature,
      Screen::ScreenCIVlimits
    };
    const Screen::ScreenType NiXXDischargeScreens[] PROGMEM = {
      Screen::ScreenFirst,
      Screen::ScreenDeltaTextern,
      Screen::ScreenR,
      Screen::ScreenVout,
      Screen::ScreenVinput,
      Screen::ScreenTime,
      Screen::ScreenTemperature,
      Screen::ScreenCIVlimits
    };

    const Screen::ScreenType theveninScreens[] PROGMEM = {
      Screen::ScreenFirst,
      Screen::ScreenBalancer1_3,            Screen::ScreenBalancer4_6,
      Screen::ScreenBalancer1_3Rth,         Screen::ScreenBalancer4_6Rth,
      Screen::ScreenR,
      Screen::ScreenVout,
      Screen::ScreenVinput,
      Screen::ScreenTime,
      Screen::ScreenTemperature,
      Screen::ScreenCIVlimits
    };
    const Screen::ScreenType balanceScreens[] PROGMEM = {
      Screen::ScreenBalancer1_3,            Screen::ScreenBalancer4_6,
      Screen::ScreenTime,
      Screen::ScreenTemperature };
    const Screen::ScreenType dischargeScreens[] PROGMEM = {
      Screen::ScreenFirst,
      Screen::ScreenBalancer1_3,            Screen::ScreenBalancer4_6,
      Screen::ScreenBalancer1_3Rth,         Screen::ScreenBalancer4_6Rth,
      Screen::ScreenR,
      Screen::ScreenVout,
      Screen::ScreenVinput,
      Screen::ScreenTime,
      Screen::ScreenTemperature,
      Screen::ScreenCIVlimits
    };
    const Screen::ScreenType storageScreens[] PROGMEM = {
      Screen::ScreenFirst,
      Screen::ScreenBalancer1_3,            Screen::ScreenBalancer4_6,
      Screen::ScreenBalancer1_3Rth,         Screen::ScreenBalancer4_6Rth,
      Screen::ScreenR,
      Screen::ScreenVout,
      Screen::ScreenVinput,
      Screen::ScreenTime,
      Screen::ScreenTemperature,
      Screen::ScreenCIVlimits
    };

    const Screen::ScreenType startInfoBalanceScreens[] PROGMEM = {
      Screen::ScreenStartInfo,
      Screen::ScreenBalancer1_3,            Screen::ScreenBalancer4_6,
      Screen::ScreenTemperature };

    const Screen::ScreenType startInfoScreens[] PROGMEM = {
      Screen::ScreenStartInfo,
      Screen::ScreenTemperature };

    void chargingComplete() {
        lcdClear();
        Screen::displayScreenProgramCompleted();
        Buzzer::soundProgramComplete();
        waitButtonPressed();
        Buzzer::soundOff();
    }

    void chargingMonitorError() {
        lcdClear();
        Screen::displayMonitorError();
        Buzzer::soundError();
        waitButtonPressed();
        Buzzer::soundOff();
    }

    bool analizeStrategyStatus(Strategy &strategy,  Strategy::statusType status, bool exitImmediately) {
        bool run = true;
        if(status == Strategy::ERROR) {
            Program::programState_ = Program::Error;
            Screen::powerOff();
            strategy.powerOff();
            chargingMonitorError();
            run = false;
        }

        if(status == Strategy::COMPLETE) {
            Program::programState_ = Program::Done;
            Screen::powerOff();
            strategy.powerOff();
            if(!exitImmediately)
                chargingComplete();
            run = false;
        }
        return run;
    }

    Strategy::statusType doStrategy(Program::ProgramState state, Strategy &strategy, const Screen::ScreenType chargeScreens[]
                                                      , uint8_t screen_limit, bool exitImmediately = false)
    {
        Program::programState_ = state;
        uint8_t key;
        bool run = true;
        uint16_t newMesurmentData = 0;
        Strategy::statusType status = Strategy::RUNNING;
        strategy.powerOn();
        Screen::powerOn();
        Monitor::powerOn();
        lcdClear();
        uint8_t screen_nr = 0;
        screen_limit--;
        do {
            if(!PolarityCheck::runReversedPolarityInfo()) {
                Screen::display(pgm::read(&chargeScreens[screen_nr]));
            }

            {
                //change displayed screen
                key =  Keyboard::getPressedWithSpeed();
                if(key == BUTTON_INC && screen_nr < screen_limit)   screen_nr++;
                if(key == BUTTON_DEC && screen_nr > 0)              screen_nr--;
            }

            if(run) {
                status = Monitor::run();
                run = analizeStrategyStatus(strategy, status, exitImmediately);

                if(run && newMesurmentData != AnalogInputs::getCalculationCount()) {
                    newMesurmentData = AnalogInputs::getCalculationCount();
                    status = strategy.doStrategy();
                    run = analizeStrategyStatus(strategy, status, exitImmediately);
                }
            }
            if(!run && exitImmediately)
                return status;
        } while(key != BUTTON_STOP);

        Screen::powerOff();
        strategy.powerOff();
        Program::programState_ = Program::None;
        return status;
    }

} //namespace {

bool Program::startInfo()
{
    if(ProgramData::currentProgramData.isLiXX()) {
        //usues balance port
        startInfoStrategy.setBalancePort(true);
        return doStrategy(Info, startInfoStrategy, startInfoBalanceScreens, sizeOfArray(startInfoBalanceScreens), true) == Strategy::COMPLETE;
    } else {
        startInfoStrategy.setBalancePort(false);
        return doStrategy(Info, startInfoStrategy, startInfoScreens, sizeOfArray(startInfoScreens), true) == Strategy::COMPLETE;
    }
}

Strategy::statusType Program::runStorage(bool balance)
{
    storageStrategy.setDoBalance(balance);
    storageStrategy.setVII(ProgramData::currentProgramData.getVoltage(ProgramData::VStorage),
            ProgramData::currentProgramData.battery.Ic, ProgramData::currentProgramData.battery.Id);
    return doStrategy(Storage, storageStrategy, storageScreens, sizeOfArray(storageScreens));
}
Strategy::statusType Program::runTheveninCharge(int minChargeC)
{
    theveninChargeStrategy.setVI(ProgramData::currentProgramData.getVoltage(ProgramData::VCharge), ProgramData::currentProgramData.battery.Ic);
    theveninChargeStrategy.setMinI(ProgramData::currentProgramData.battery.Ic/minChargeC);
    return doStrategy(Charging, theveninChargeStrategy, theveninScreens, sizeOfArray(theveninScreens));
}

Strategy::statusType Program::runTheveninChargeBalance(int minChargeC)
{
    chargeBalanceStrategy.setVI(ProgramData::currentProgramData.getVoltage(ProgramData::VCharge), ProgramData::currentProgramData.battery.Ic);
    return doStrategy(ChargingBalancing, chargeBalanceStrategy, theveninScreens, sizeOfArray(theveninScreens));
}


Strategy::statusType Program::runDeltaCharge()
{
    deltaChargeStrategy.setTestTV(settings.externT_, true);
    return doStrategy(Charging, deltaChargeStrategy, deltaChargeScreens, sizeOfArray(deltaChargeScreens));
}

Strategy::statusType Program::runDischarge()
{
    theveninDischargeStrategy.setVI(ProgramData::currentProgramData.getVoltage(ProgramData::VDischarge), ProgramData::currentProgramData.battery.Id);
    return doStrategy(Discharging, theveninDischargeStrategy, dischargeScreens, sizeOfArray(dischargeScreens));
}

Strategy::statusType Program::runNiXXDischarge()
{
    theveninDischargeStrategy.setVI(ProgramData::currentProgramData.getVoltage(ProgramData::VDischarge), ProgramData::currentProgramData.battery.Id);
    return doStrategy(Discharging, theveninDischargeStrategy, NiXXDischargeScreens, sizeOfArray(NiXXDischargeScreens));
}

Strategy::statusType Program::runBalance()
{
    return doStrategy(Balancing, balancer, balanceScreens, sizeOfArray(balanceScreens));
}

void Program::run(ProgramType prog)
{
    programType_ = prog;
    stopReason_ = PSTR("");

    if(startInfo()) {
        Buzzer::soundStartProgram();

        switch(prog) {
        case Program::ChargeLiXX:
            runTheveninCharge(10);
            break;
        case Program::Balance:
            runBalance();
            break;
        case Program::DischargeLiXX:
            runDischarge();
            break;
        case Program::FastChargeLiXX:
            runTheveninCharge(5);
            break;
        case Program::StorageLiXX:
            runStorage(false);
        case Program::StorageLiXX_Balance:
            runStorage(true);
            break;
        case Program::ChargeNiXX:
            runDeltaCharge();
            break;
        case Program::DischargeNiXX:
            runNiXXDischarge();
            break;
        case Program::ChargeLiXX_Balance:
            runTheveninChargeBalance(10);
            break;
        default:
            //TODO:
            Screen::runNotImplemented();
            break;
        }
    }
}



// Program selection depending on the battery type

namespace {

    const char charge_str[] PROGMEM = "charge";
    const char chaBal_str[] PROGMEM = "charge+balance";
    const char balanc_str[] PROGMEM = "balance";
    const char discha_str[] PROGMEM = "discharge";
    const char fastCh_str[] PROGMEM = "fast charge";
    const char storag_str[] PROGMEM = "storage";
    const char stoBal_str[] PROGMEM = "storage+balanc";
    const char CDcycl_str[] PROGMEM = "c>d cycle";
    const char DCcycl_str[] PROGMEM = "d>c cycle";
    const char edBatt_str[] PROGMEM = "edit battery";

    const char * const programLiXXMenu[] PROGMEM =
    { charge_str,
      chaBal_str,
      balanc_str,
      discha_str,
      fastCh_str,
      storag_str,
      stoBal_str,
      edBatt_str
    };

    const Program::ProgramType programLiXXMenuType[] PROGMEM =
    { Program::ChargeLiXX,
      Program::ChargeLiXX_Balance,
      Program::Balance,
      Program::DischargeLiXX,
      Program::FastChargeLiXX,
      Program::StorageLiXX,
      Program::StorageLiXX_Balance,
      Program::EditBattery
    };

    const char * const programNiXXMenu[] PROGMEM =
    { charge_str,
      discha_str,
      CDcycl_str,
      DCcycl_str,
      edBatt_str
    };

    const Program::ProgramType programNiXXMenuType[] PROGMEM =
    { Program::ChargeNiXX,
      Program::DischargeNiXX,
      Program::CDcycleNiXX,
      Program::DCcycleNiXX,
      Program::EditBattery
    };

    const char * const programPbMenu[] PROGMEM =
    { charge_str,
      discha_str,
      edBatt_str
    };

    const Program::ProgramType programPbMenuType[] PROGMEM =
    { Program::ChargePb,
      Program::DischargePb,
      Program::EditBattery
    };


    StaticMenu selectLiXXMenu(programLiXXMenu, sizeOfArray(programLiXXMenu));
    StaticMenu selectNiXXMenu(programNiXXMenu, sizeOfArray(programNiXXMenu));
    StaticMenu selectPbMenu(programPbMenu, sizeOfArray(programPbMenu));

    StaticMenu * getSelectProgramMenu() {
        if(ProgramData::currentProgramData.isLiXX())
            return &selectLiXXMenu;
        else if(ProgramData::currentProgramData.isNiXX())
            return &selectNiXXMenu;
        else return &selectPbMenu;
    }
    Program::ProgramType getSelectProgramType(uint8_t index) {
        const Program::ProgramType * address;
        if(ProgramData::currentProgramData.isLiXX())
            address = &programLiXXMenuType[index];
        else if(ProgramData::currentProgramData.isNiXX())
            address = &programNiXXMenuType[index];
        else address = &programPbMenuType[index];
        return pgm::read(address);
    }
}

void Program::selectProgram(int index)
{
    ProgramData::loadProgramData(index);
    StaticMenu * selectPrograms = getSelectProgramMenu();
    int8_t menuIndex;
    do {
        menuIndex = selectPrograms->runSimple();
        if(menuIndex >= 0)  {
            Program::ProgramType prog = getSelectProgramType(menuIndex);
            if(prog == Program::EditBattery) {
                if(ProgramData::currentProgramData.edit(index)) {
                    Buzzer::soundSave();
                    ProgramData::saveProgramData(index);
                    selectPrograms = getSelectProgramMenu();
                }
            } else {
                Program::run(prog);
            }
        }
    } while(menuIndex >= 0);
}




