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
// Libraries for SD card
  #include "FS.h"
  #include "SD.h"
  #include <SPI.h>
  // Define CS pin for the SD card module
  #define SD_CS 5
  // String for writing data to SD card and serial port
  String dataMessage;


// Libraries for WiFi
  #include <WiFi.h>
  //*** Replace with your network credentials ***********************************
  const char* ssid     = "Smart Village-Ormoti";//"iPhone XR";
  const char* password = "sm@rtvill@ge";//"smartvils";


//Variabls to save date and time for RTC
  char dateStamp[] = "YYYY-MM-DD";
  char timeStamp[] = "hh:mm:ss";
  
//Libraries for RTC Module
  #include "RTClib.h"
  RTC_DS3231 rtc;
  //Variables to save date and time for RTC
  String dateStamp;
  String timeStamp;

// Save reading number on ESP32 RTC memory
  RTC_DATA_ATTR int readingID = 0;

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

//ThingSpeak server settings
//REPLACE WITH YOUR THINGSPEAK CHANNEL NUMBER AND API KEY IF DIFFERENT*****************************
  #include <ThingSpeak.h>
  unsigned long myChannelNumber = 1552034; 
    //1552004 for white box with 30A YHDC sensor
    //1552034 for clear box with 30A magnelab
    //1552039 for clear box with 50A magnelab
  const char * myWriteAPIKey = "7FD05YWSW73TJ0RS"; 
    //JTHI50B1U0SSHBYP for white box with 30A YHDC sensor
    //7FD05YWSW73TJ0RS for clear box with 30A magnelab
    //BWG36G64JRRI6AXG for clear box with 50A magnelab
  WiFiClient  client;

//Library for AC Voltage and Current calculations
  //note this is not the standard EmonLib as it has been adapted with a callback instead of directly reading an analog pin
  #include "EmonLibADC.h"

//Create instance of EmonLib EnergyMonitor Class for each phase
  EnergyMonitor emon1;

  const float I_AC100_Calibration = 52.28; //********May need tuning (Should be sensor current when have 1V on ADC)**************** Use 30 for 30A-1V, use 111.11 for 18R and 100:50mA

  // 18.1 for white box with 30A YHDC sensor (calibrated at 5.6A. Theoretical value should be 30, so it is a bit weird...)
  //52.28 for clear box with 30A Magnelab sensor (calibrated at 3A). Theoretical value 90.1
  //87.4 for clear box with 50A Magnelab sensor (Calibrated at 3A). Theoretical value 150.1)


//Setup Pin for debugging LED
  const int ledRedPin = 4;
  const int ledGreenPin = 16;

  
void setup() {
  // Start serial communication for debugging purposes
  Serial.begin(115200);
  
  //Set up LEDs
  pinMode (ledRedPin, OUTPUT);
  pinMode (ledGreenPin, OUTPUT);
  digitalWrite (ledRedPin, HIGH);  // turn on the red LED for turning on
  
  //start MCP3008 connection
  mcp.begin(_SCLK, _MISO, _MOSI, _SS);   

  //Connect to Wifi
  WiFiSetup();

  //set Up SD card (check for card, write headings, prepare for writing values etc.)
  setUpSDCard();

  //Set up RTC with laptop time if first time using.
  setUpRTC();

  ThingSpeak.begin(client);  // Initialize ThingSpeak

  //Set up ADC sensors
  setUpACSensors();

  digitalWrite (ledRedPin, LOW);  // turn on the red LED for turning on
  digitalWrite (ledGreenPin, HIGH);  // turn on the LED
}



void loop() {
  // Increment readingID on every new reading 
  readingID++;
  getReadings();
  getTimeStamp();
  logSDCard();
  updateThingSpeak();
  delay(30000);
}

// Function to update all ADC readings
void getReadings(){
  updateACValues();
}

void WiFiSetup(){
    // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    digitalWrite (ledRedPin, LOW);  // turn off the error LED
    delay(250);
    digitalWrite (ledRedPin, HIGH);  // turn on the error LED
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  digitalWrite (ledGreenPin, HIGH);  // turn on the LED
}

void setUpRTC(){
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
  }

//  if (! rtc.lostPower()) {
//    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
//    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
//  }
}

// Function to get date and time from RTC module
void getTimeStamp() {
    DateTime time = rtc.now();
    dateStamp = time.timestamp(DateTime::TIMESTAMP_DATE);
    timeStamp = time.timestamp(DateTime::TIMESTAMP_TIME);
}

//set Up SD Card. Write data headings if new file.
void setUpSDCard(){
  // Initialize SD card
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
    return;    // init failed
  }

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.txt");
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "ReadingID, dayStamp, timeStamp, ACCurrent \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
}

// Write the sensor readings on the SD card
void logSDCard() {
   SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
    return;    // init failed
  } 
  dataMessage = String(readingID) + "," + String(dateStamp) + "," + String(timeStamp) + "," + 
         String(I_AC100) + "\r\n";
  Serial.print("Saving data to SD Card: ");
  Serial.println(dataMessage);
  appendFile(SD, "/data.txt", dataMessage.c_str());
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);
  
  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
    digitalWrite (ledRedPin, HIGH);  // turn on the LED
  }
  file.close();
}


void updateThingSpeak(){
  //set what we want each ThingsSpeak fields and statuses to be updated to 
  ThingSpeak.setField(1,I_AC100);

  ThingSpeak.setStatus("no dump load triggered");
  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("ThingSpeak Channel update successful.");
   }
  else if(x == -401){
    Serial.println("Failed to write to ThingSpeak. Most probable cause is the rate limit of once every 15 seconds exceeded");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
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
}
