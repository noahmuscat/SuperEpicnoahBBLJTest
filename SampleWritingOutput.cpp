
#include "BBLJ.hpp"
/**
 * Name: SampleWritingOutput
 * Desc: Copy of BBLJ::writeOutputPinValues with added pulse generator
**/

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>  // For inet_ntoa()
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <LabJackM.h>
#include <time.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>

#include "External/C_C++_LJM/LJM_Utilities.h"
#include "External/C_C++_LJM/LJM_StreamUtilities.h" // Include the Stream utilities now

#include "FilesystemHelpers.h"
#include "LabjackHelpers.h"
//#include "LabjackStreamHelpers.h"
//#include "LabjackStreamInfo.h"

#include "LabjackLogicalInputChannel.h"
#include "WindowsHelpers.h"

// In all other files
//#define PFD_SKIP_IMPLEMENTATION 1
#include "External/portable-file-dialogs.h"

// Set to non-zero for external stream clock
#define EXTERNAL_STREAM_CLOCK 0

// Set FIO0 to pulse out. See EnableFIO0PulseOut()
#define FIO0_PULSE_OUT 0


void BehavioralBoxLabjack::writeOutputPinValues()
{
    this->writeOutputPinValues(false);
}
void BehavioralBoxLabjack::writeOutputPinValues(bool shouldForceWrite)
{
    auto writeTime = Clock::now();

    //Loop through and write the values that have changed
    // Iterate through the output ports
    for (int i = 0; i < outputPorts.size(); i++)
    {
        // Get the appropriate value for the current port (calculating from the saved lambda function).
        double outputValue = outputPorts[i]->getValue();

        // Check to see if the value changed, and if it did, write it.
        bool didChange = outputPorts[i]->set(writeTime, outputValue);

        if (didChange || shouldForceWrite) {
            // Get the c_string name to pass to the labjack write function
            std::string portNameString = outputPorts[i]->getPinName();
            const char* portName = portNameString.c_str();

            // Set DIO state on the LabJack
            //TODO: the most general way to handle this would be to have each pin have a lambda function stored that sets the value in an appropriate way. This is currently a workaround.
            bool isVisibleLightRelayPort = (i == 0);
            bool setPulseOutput = (i == 1);
            if (isVisibleLightRelayPort) {
                if (outputValue == 0.0) {
                    // a value of 0.0 means that the light should be on, so it should be in output mode.
                    
                    // Lock the mutex to prevent concurrent labjack interaction
                    std::lock_guard<std::mutex> labjackLock(this->labjackMutex);
                    this->err = LJM_eWriteName(this->handle, portName, outputValue);
                    ErrorCheck(this->err, "LJM_eWriteName");
                }
                else {
                    // a value greater than 0.0 means that the light should be off, so it should be set to input mode. This is accomplished by reading from the port (instead of writing).
                    // Lock the mutex to prevent concurrent labjack interaction
                    std::lock_guard<std::mutex> labjackLock(this->labjackMutex);
                    double tempReadValue = 0.0;
                    this->err = LJM_eReadName(this->handle, portName, &tempReadValue);
                    ErrorCheck(this->err, "LJM_eReadName");
                    //std::cout << "\t Visible LED Override: read from port. " << tempReadValue << std::endl;
                }
            }
            else if (setPulseOutput) {
                
                // Not the visible light relay and can be handled in the usual way.
                // RIGHT HERE IS WHERE TO WRITE THE PULSE!!!!!!!!
                
                // Lock the mutex to prevent concurrent labjack interaction
                std::lock_guard<std::mutex> labjackLock(this->labjackMutex);
                
                // the handle is already taken care of, as well as the portName (gotten from the iteration above)
                // will we need to first adjust the vector of outputPorts? Not sure... --> done in code below
                
                this->err = LJM_eWriteName(this->handle, portName, outputValue);
                ErrorCheck(this->err, "LJM_eWriteName");
            
            }
            else {
                // Not the visible light relay and can be handled in the usual way
                // Lock the mutex to prevent concurrent labjack interaction
                std::lock_guard<std::mutex> labjackLock(this->labjackMutex);
                this->err = LJM_eWriteName(this->handle, portName, outputValue);
                ErrorCheck(this->err, "LJM_eWriteName");
            }
            
            if (PRINT_OUTPUT_VALUES_TO_CONSOLE) {
                printf("\t Set %s state : %f\n", portName, outputValue);
            }
            
        } // end if didChange
    } // end for
}



// editing here for line 158 - 182 in BBLJ.cpp
std::function<double()> visibleLEDRelayFunction = [=]() -> double {
    if (this->isVisibleLEDLit()) { return 0.0; }
    else { return 1.0; }
};
//std::function<double(int)> drinkingPortAttractorModeFunction = [=](int portNumber) -> double {
//    if (!this->isAttractModeLEDLit(portNumber)) { return 0.0; }
//    else { return 1.0; }
//};


std::function<double()> pulseGeneratorVoltage = [=]() -> double {
    // could adjust the desired voltage here
    return 5.0;
};


// Create output ports for all output ports (TODO: make dynamic)
for (int i = 0; i < NUM_OUTPUT_CHANNELS; i++) {
    std::string portName = std::string(outputPortNames[i]);
    OutputState* currOutputPort;
    if (i == 0) {
        currOutputPort = new OutputState(portName, visibleLEDRelayFunction);
    }
    
    // attempting to add a specific outputPort for the pulse
    // pulse value attempting to be set at 5.0 V
    else if (i == 1) {
        currOutputPort = new OutputState(portName, pulseGeneratorVoltage);
    }
    
    // not sure what is exactly happening here
    // appears to be configuration all other output ports
    else {
        std::function<double()> drinkingPortAttractorModeFunction = [=]() -> double {
            if (!this->isAttractModeLEDLit(i)) { return 0.0; }
            else { return 1.0; }
        };
        currOutputPort = new OutputState(portName, drinkingPortAttractorModeFunction);
    }
    
    this->outputPorts.push_back(currOutputPort);
}
