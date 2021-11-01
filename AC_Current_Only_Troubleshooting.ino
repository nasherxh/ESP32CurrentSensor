/*********
  ESP32
  Connect to MCP3008 ADC chip via HSPI
  Connect to SD card via VSPI (IO23=MOSI, IO18=SCK,IO19=MISO,IO5=CS)
  Connect to Wifi to upload data to ThingSpeak Channel --> 
    Note that ThingSpeak has only 8 channels, and there is a rate limit of 1 reading every 15 seconds.
  Write data from MCP3008 to SD card.
  Get time from RTC Module

  Uses forked EmonLib library with callback method, enabling use of external adc
    Library fork available here: https://github.com/PaulWieland/EmonLib/tree/4a965d87061c19ad8b0a35bb173caada014ecbd9

  Remember to tie GPIO2 to GPIO0 if using for HSPI (to allow flashing ESP32)

  Note that if power is lost momentarily from SD card, it will fail for all subsequent appends
  ---> Need to find a way of restarting connection each time such that this is not an issue 
  (I think it is software-fixable as restarting the program fixes the problem)

*********/

//Library for MCP3008 ADC chip
  #include <mcp3008.h>
  //initiate an instance of MCP3008
  mcp3008 mcp = mcp3008();
  //Setup pins for ADC SPI interface. Example shows HSPI options*************************************************
  const uint8_t _SS   = 13;//15;
  const uint8_t _MISO = 2;//12;//Note that if using '2', must tie pins 2 and 0 together. If using 12, remove connection during code upload.
  const uint8_t _MOSI = 15;//13;
  const uint8_t _SCLK = 14;   

  const int I_AC100_Pin = 4; //Current sensor attached to ADC pin 4
  
//Calculated Sensor Values (shown below in order of ADC pin assignment)
  float I_AC100;

//Library for AC Voltage and Current calculations
  //note this is not the standard EmonLib as it has been adapted with a callback instead of directly reading an analog pin
  #include "EmonLibADC.h"

//Create instance of EmonLib EnergyMonitor Class for each phase
  EnergyMonitor emon1;

  const float I_AC100_Calibration = 18.1;//52.28;//87.4;//111.11;//23.5; 
    //18.1 for white box with 30A YHDC sensor (calibrated at 5.6A) Theoretical value 30
    //52.28 for clear box with 30A Magnelab sensor (calibrated at 3A). Theoretical value 90.1
    //87.4 for clear box with 50A Magnelab sensor (Calibrated at 3A). Theoretical value 150.1)
  
void setup() {
  // Start serial communication for debugging purposes
  Serial.begin(115200);

  //start MCP3008 connection
  mcp.begin(_SCLK, _MISO, _MOSI, _SS);   
  
  //Set up ADC sensors
  setUpACSensors();
}


void loop() {
  getReadings();
  delay(1000);
}

// Function to update all ADC readings
void getReadings(){
  updateACValues();
}


// Callback method for reading the pin value from the MCP instance (needed for EmonLib) 
int MCP3008PinReader(int _pin){
  return mcp.analogRead(_pin);
}

void setUpACSensors(){
  emon1.inputPinReader = MCP3008PinReader; // Replace the default pin reader with the customized ADC pin reader
  emon1.current(I_AC100_Pin, I_AC100_Calibration);               // Current: input pin, calibration.
}


void updateACValues(){
  I_AC100           = emon1.calcIrms(2000);              //extract Irms into Variable
  Serial.println(I_AC100);     
}
