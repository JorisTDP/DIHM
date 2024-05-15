#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "base64.hpp"
#include "config.h"
#include "HT_st7735.h"
#include "Arduino.h"
HT_st7735 st7735;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool dataSent = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
const char *uplinkMessage = "Sending...";
uint8_t downlinkData[] = {
    0x7C, 0x20, 0x55, 0x73, 0x65, 0x72, 0x3A, 0x20, 0x4A, 0x6F,0x74
}; 
uint8_t storedData[64];

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {
  USBSerial.begin(115200);
  st7735.st7735_init();
  st7735.st7735_fill_screen(ST7735_BLACK);
  BLEDevice::init("ESP32-DIHM-MODULE");
  st7735.st7735_write_str(0, 0, "--init radio---", Font_7x10, ST7735_RED, ST7735_BLACK);
  int state = radio.begin();
  debug(state != RADIOLIB_ERR_NONE, F("Initalise radio failed"), state, true);
  state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey, true);
  if(state < RADIOLIB_ERR_NONE) {st7735.st7735_write_str(0, 0, "--failed---", Font_7x10, ST7735_RED, ST7735_BLACK);state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey, true);}
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  dataSent = false;
  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  USBSerial.println("Waiting a client connection to notify...");
  // Build payload byte array
}

void loop() {
  if (deviceConnected) {
        //st7735.st7735_write_str(0, 0, "--BLE connect---", Font_7x10, ST7735_RED, ST7735_BLACK);
        if(dataSent == false){ sendLora(); USBSerial.println("SENDING REQ...");} //Example byte array
        size_t arrayLength = sizeof(storedData) / sizeof(storedData[0]); 
        unsigned char unsignedCharArray[arrayLength];
        byteArrayToUnsignedCharArray(storedData, unsignedCharArray, arrayLength);
        for (int i = 0; i < sizeof(storedData); i++) {
          USBSerial.print(storedData[i], HEX); // Print the byte in hexadecimal format
          USBSerial.print(" "); // Print a space between bytes
      }
        USBSerial.println();
        const char* convertedData = reinterpret_cast<const char*>(unsignedCharArray);
        USBSerial.println(convertedData);
        st7735.st7735_write_str(0, 0, convertedData, Font_7x10, ST7735_RED, ST7735_BLACK);
        sendData(unsignedCharArray); // sendData(lora_downlink);
        delay(1000); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        USBSerial.println("start advertising");
        dataSent = false;
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
        dataSent = false;
    }
}

void sendLora(){
    size_t downlinkSize = 10;
    int timeOut = 500; //3 sec?
    int state = 1;//node.sendReceive(uplinkMessage, 1, downlinkData, &downlinkSize, true); //uplink and downlink same function    
    debug((state != RADIOLIB_LORAWAN_NO_DOWNLINK) && (state != RADIOLIB_ERR_NONE), F("Error in sendReceive"), state, false);
    memcpy(storedData, downlinkData, downlinkSize); // Copy received downlink data to storedData
//    unsigned long startTime = millis();  // Record the start time
//    while (millis() - startTime < timeOut) {
//        int state = node.downlink(downlinkData, &downlinkSize);
//        if (downlinkSize + sizeof(storedData) < sizeof(storedData)) { // Check if there is enough space
//            memcpy(storedData + downlinkSize, downlinkData, downlinkSize); // Append downlinkData to storedData
//        } else {
//            //debug(true, F("storedData is full"), false);
//            break; // Exit the loop, storedData is full
//        }
//      }
}

void stringToUnsignedCharArray(const String &input, unsigned char output[], size_t maxLength) {
  size_t length = input.length();
  if (length > maxLength) {
    length = maxLength;
  }
  for (size_t i = 0; i < length; ++i) {
    output[i] = (unsigned char)input.charAt(i);
  }
}

void byteArrayToUnsignedCharArray(uint8_t *data, unsigned char *result, size_t len) {
    for (size_t i = 0; i < len; i++) {
        result[i] = static_cast<unsigned char>(data[i]);
    }
}

void sendData(unsigned char inp[]){
    //fullstring = given bij LoRa downlink and should be decode from base64
//    String fullString = "fCBVc2VyOiBKb3JpcywgRGVzY3I6IFVwbG9hZCAxLCBGaWxlOiBBZnN0dWRlZXJPcGRyYWNodF9TY3JpcHRpZS5wZGYsIFRpbWU6IDIwMjQtMDQtMDIgMTQ6MTY6MTQgfCBVc2VyOiBKb3JpcywgRGVzY3I6IElobSB2ZXJzaWUgMiwNCg0KLi4uIA0KDQouLi4sIEZpbGU6IFJFQURNRS5tZCwgVGltZTogMjAyNC0wNC0wMyAxMjoxNToxMyA=";
//    int len = fullString.length();
//    unsigned char charred[1000];
//    stringToUnsignedCharArray(fullString,charred,1000); 
//    unsigned char decoded_text[1000];
//    int decoded_length = decode_base64(inp,decoded_text);
//    String fullString = (char *)decoded_text
    String fullString = (char *)inp;
    int len = fullString.length();
    int chunkSize = 20;
    if(dataSent == false){
      USBSerial.println("sending data over ble"); 
      for (int i = 0; i < len; i += chunkSize) {
    // Get the substring of the full string for this chunk
      String chunk = fullString.substring(i, min(i + chunkSize, len));
      // Set the value of the characteristic to the chunk
      pCharacteristic->setValue(chunk.c_str());
      // Notify the characteristic
      pCharacteristic->notify();
      // Delay to avoid congestion in the Bluetooth stack
      delay(5); // You may adjust this delay as needed
     }
     dataSent = true;
  }
    
}
