#include <Arduino.h>
#include <SPIFFS.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <string>
#include <ArduinoJson.h>

const int MAX_CONNECTION_ATTEMPTS = 7;
const int MAX_NETWORKS_SHOWN = 10;

const long SCAN_PERIOD = 5000;

#define EEPROM_SIZE 97
#define EEPROM_SSID_ADDRESS 0
#define EEPROM_PASSWORD_ADDRESS 32
#define EEPROM_CONNECTION_FLAG_ADDRESS 96

class SoftAP
{
public:
    SoftAP();
    void SetupSoftAP();
    bool Connect(String network_ssid, String network_password);
    bool Disconnect();
    bool Reconnect();
    void HandleGetRequest(AsyncWebServerRequest *request);
    void CheckConnectionRequest(AsyncWebServerRequest *request);
    void SearchForAPsRequest(AsyncWebServerRequest *request);
    void SaveConnectionData(String ssid, String password);

private:
    String connected_to;
};