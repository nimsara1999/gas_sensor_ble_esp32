// #include <Arduino.h>
// #include <BLEDevice.h>
// #include <BLEUtils.h>
// #include <BLEScan.h>
// #include <BLEAdvertisedDevice.h>
// #include <WiFi.h>
// #include <NetworkClient.h>
// #include <WiFiAP.h>

// BLEScan *pBLEScan;

// const int scanTimeSeconds = 1;

// const char *ssid = "yourAP";
// const char *password = "12345678";

// std::string string_to_hex(const std::string &input)
// {
//     static const char hex_digits[] = "0123456789abcdef";
//     std::string output;
//     output.reserve(input.length() * 2);
//     for (unsigned char c : input)
//     {
//         output.push_back(hex_digits[c >> 4]);
//         output.push_back(hex_digits[c & 15]);
//     }
//     return output;
// }

// std::string format_hex_string(const std::string &hexString)
// {
//     std::string formattedString;
//     for (size_t i = 0; i < hexString.length(); i += 2)
//     {
//         formattedString += hexString.substr(i, 2);
//         if (i + 2 < hexString.length())
//         {
//             formattedString += " ";
//         }
//     }
//     return formattedString;
// }

// int hex_to_int(const std::string &hexString)
// {
//     return strtol(hexString.c_str(), nullptr, 16);
// }

// class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
// {
//     void onResult(BLEAdvertisedDevice advertisedDevice)
//     {
//         if (strcmp(advertisedDevice.getName().c_str(), "") == 0)
//         {
//             std::string hexAdvData = string_to_hex(advertisedDevice.getManufacturerData());
//             if (hexAdvData.rfind("544e", 0) == 0)
//             {
//                 std::string frameHead1 = hexAdvData.substr(0, 4);
//                 std::string type = hexAdvData.substr(4, 2);
//                 std::string cmd = hexAdvData.substr(6, 2);
//                 std::string frameHead2 = hexAdvData.substr(8, 4);
//                 std::string measurementHex = hexAdvData.substr(12, 4);
//                 std::string batteryHex = hexAdvData.substr(16, 2);

//                 int measurement = hex_to_int(measurementHex);
//                 int battery = hex_to_int(batteryHex);

//                 Serial.printf("*********************\n");
//                 Serial.printf("Received Data: %s\n", format_hex_string(hexAdvData).c_str());
//                 Serial.printf("Frame Head: %s\n", frameHead1.c_str());
//                 Serial.printf("Type: %s\n", type.c_str());
//                 Serial.printf("Cmd: %s\n", cmd.c_str());
//                 Serial.printf("Measurement Result (US): %d\n", measurement);
//                 Serial.printf("Battery: %d % \n", battery);
//                 Serial.printf("RSSI: %d\n", advertisedDevice.getRSSI());
//                 Serial.printf("Prepared packet: \n");
//                 Serial.printf("{\"DATETIME\":1724343139511,\"IMEI\":\"A4C138CCD9ED\",\"NCU_FW_VER\":109,\"GAS_METER\":%d,\"CSQ\":104,\"MCU_TEMP\":30,\"BAT_VOL\":%d,\"METER_TYPE\":4,\"TIME_ZONE\":\"+07\",\"TANK_SIZE\":\"20lb\",\"GAS_PERCENT\":0}\n\n", measurement, battery);
//             }
//         }
//     }
// };

// void setup()
// {
//     Serial.begin(115200);

//     Serial.println("Scanning for sensors...");

//     BLEDevice::init("");
//     pBLEScan = BLEDevice::getScan();
//     pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
//     pBLEScan->setActiveScan(false);
//     pBLEScan->setInterval(100);
//     pBLEScan->setWindow(99);
// }

// void loop()
// {
//     BLEScanResults foundDevices = pBLEScan->start(scanTimeSeconds, false);
//     pBLEScan->clearResults();
// }
