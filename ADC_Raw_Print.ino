/*

   ESP32 Arduino Library for:
	
   MCP3008 Chip 

   Provides 8 additional Analog inputs, 10 bits each 

   Uses Hardware SPI Interface

   05/2019 Bill Row

  Note pin 12 needs to be pulled low on startup.
    It has an internal pull down resistor but if attached to MISO this doesn't work
    Need to remove connection during upload/boot stage.
  NOte pin 2 also needs to be pulled low at startup
    If using pin 2, can attach jumper between 0 and 2, and this will pull pin 2 low
    This works so can leave all connections in on startup.


*/

#include <mcp3008.h>

mcp3008 mcp = mcp3008();

////for VSPI
//const uint8_t _SS   = 5;;   // Pins for SPI interface
//const uint8_t _MISO = 19;
//const uint8_t _MOSI = 23;
//const uint8_t _SCLK = 18;   


//for HSPI
const uint8_t _SS   = 13;//15;   // Pins for SPI interface
const uint8_t _MISO = 2;//12;     
const uint8_t _MOSI = 15;//13;   // example shows HSPI defaults. Two options.
const uint8_t _SCLK = 14;   


void setup() {

  Serial.begin(115200);

  mcp.begin(_SCLK, _MISO, _MOSI, _SS);      
     
}

void loop() {

//  for (int pin_ = 0; pin_ < 8; pin_++) {
//
//    uint16_t value = mcp.analogRead(pin_);
//
//    Serial.println(value);
//
//  }

uint16_t value = mcp.analogRead(0);
Serial.println(value);
  delay(200);

}
