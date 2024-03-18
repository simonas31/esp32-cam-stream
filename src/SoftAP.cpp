#include "SoftAP.h"

const String SoftAP_ssid = "ElderWatch-1234";
const String SoftAP_password = "123456789";

const IPAddress local_IP(192, 168, 1, 22);
const IPAddress gateway(192, 168, 1, 5);
const IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);

SoftAP::SoftAP()
{
    Serial.begin(115200);

    // Initialization of SoftAP to be able to connect to specified WiFi network
    // WiFi.mode(WIFI_MODE_AP);
    // WiFi.softAPConfig(local_IP, gateway, subnet);

    WiFi.begin("TP-Link_4FD6", "53503794");
    // WiFi.softAP(SoftAP_ssid, SoftAP_password);

    while (WiFi.status() != WL_CONNECTED)
    { // wait until WiFi is connected
        delay(1000);
        Serial.print(".");
    }

    Serial.print("SoftAP IP address = ");
    Serial.println(WiFi.localIP());

    if (!SPIFFS.begin(true))
    {
        Serial.println("An error occurred while mounting SPIFFS");
        return;
    }

    server.on("/", HTTP_GET, std::bind(&SoftAP::HandleGetRequest, this, std::placeholders::_1));

    server.on(
        "/connect", HTTP_POST, [](AsyncWebServerRequest *request)
        {
            // This is the main handler for handling the POST request itself
            // You can put your main POST request handling logic here
        },
        [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            // This is the onBody handler for processing request body
            // You can put your request body handling logic here
        },
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            Serial.begin(115200);
            if (index == 0)
            {
                // Initialize a string buffer to hold the incoming data
                static String postData;

                // Convert the incoming data to a string
                postData = String((char *)data);

                // Check if this is the final chunk of data
                if (index + len == total)
                {
                    // If this is the final chunk, then we have received all the data
                    // Now you can parse the entire data and perform further actions

                    // Parse the JSON data using ArduinoJson
                    JsonDocument doc; // Adjust the size as needed
                    DeserializationError error = deserializeJson(doc, postData);

                    // Check if parsing was successful
                    if (error)
                    {
                        // Parsing failed
                        Serial.print("deserializeJson() failed: ");
                        Serial.println(error.c_str());
                        // Respond with an error to the client
                        request->send(400, "text/plain", "Failed to parse JSON data");
                    }

                    // Parsing successful
                    // Extract values from the JSON document
                    const char *network_password = doc["network_password"];
                    const char *network_ssid = doc["network_ssid"];

                    // Print the extracted values for debugging
                    Serial.print("Network SSID: ");
                    Serial.println(network_ssid);
                    Serial.print("Network Password: ");
                    Serial.println(network_password);
                }
            }
        });
    server.begin();
}

void SoftAP::HandleGetRequest(AsyncWebServerRequest *request)
{
    Serial.begin(115200);
    File file = SPIFFS.open("/index.html", "r");
    if (!file)
    {
        Serial.println("Failed to open");
        request->send(404, "text/plain", "Failed to open index.html");
        return;
    }

    // Read file content into a String
    String htmlContent = file.readString();

    // Replace placeholder with desired string
    // update network access points
    String *networks = SearchForAPs();
    String options = "";
    if (networks != nullptr)
    {
        for (int i = 0; i < number_of_networks; ++i)
        {
            options += "<option>" + networks[i] + "</option>";
        }
    }
    htmlContent.replace("$OPTIONS$", options);

    file.close();
    // Send modified content as response
    request->send(200, "text/html", htmlContent);
}

/// @brief Connects to WiFi network
bool SoftAP::Connect()
{
    return false;
}

/// @brief Disconnects from WiFi network
bool SoftAP::Disconnect()
{
    return false;
}

/// @brief Reconnect to WiFi network
/// @return
bool SoftAP::Reconnect()
{
    return false;
}

/// @brief Search fro WiFi networks
/// @return
String *SoftAP::SearchForAPs()
{
    number_of_networks = WiFi.scanNetworks();
    networks_ssids = new String[number_of_networks];

    for (int i = 0; i < number_of_networks; ++i)
    {
        networks_ssids[i] = WiFi.SSID(i);
    }

    return networks_ssids;
}