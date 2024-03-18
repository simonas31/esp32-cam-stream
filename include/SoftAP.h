#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <string>
#include <ArduinoJson.h>

class SoftAP
{
public:
    SoftAP();
    bool Connect();
    bool Disconnect();
    bool Reconnect();
    void HandleGetRequest(AsyncWebServerRequest *request);
    String *SearchForAPs();

private:
    int number_of_networks;
    String *networks_ssids;
    String connected_to;
};