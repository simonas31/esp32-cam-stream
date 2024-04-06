#include "SoftAP.h"

const String SoftAP_ssid = "ElderWatch-1234";
const String SoftAP_password = "123456789";

const IPAddress local_IP(192, 168, 1, 22);
const IPAddress gateway(192, 168, 1, 5);
const IPAddress subnet(255, 255, 255, 0);

AsyncWebServer async_server(80);

long lastScanMillis = 0;

SoftAP::SoftAP()
{
    Serial.begin(115200);

    if (!EEPROM.begin(EEPROM_SIZE))
    {
        Serial.println("An error occurred while initializing EEPROM");
        ESP.restart();
        return;
    }

    if (!SPIFFS.begin(true))
    {
        Serial.println("An error occurred while mounting SPIFFS");
        ESP.restart();
        return;
    }

    // first setup soft ap
    SetupSoftAP();

    // then try to connect to wifi
    String ssid, password;
    byte connection_flag = EEPROM.read(EEPROM_CONNECTION_FLAG_ADDRESS);

    if (connection_flag == 1)
    {
        Serial.println("Previous connection detected. Restoring connection...");
        EEPROM.get(EEPROM_SSID_ADDRESS, ssid);
        EEPROM.get(EEPROM_PASSWORD_ADDRESS, password);
        Connect(ssid, password);
    }
    else
    {
        Serial.println("No previous connection detected.");
    }

    async_server.on("/", HTTP_GET, std::bind(&SoftAP::HandleGetRequest, this, std::placeholders::_1));

    async_server.on("/checkConnection", HTTP_GET, std::bind(&SoftAP::CheckConnectionRequest, this, std::placeholders::_1));

    async_server.on("/networks", HTTP_GET, std::bind(&SoftAP::SearchForAPsRequest, this, std::placeholders::_1));

    async_server.on(
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

                    // not connected
                    if (!WiFi.isConnected())
                    {
                        this->Connect(network_ssid, network_password) ? request->send(200, "text/plain", "Ok") : request->send(200, "text/plain", "Failed to Connect");
                        return;
                    }

                    // trying to connect to another network
                    int str_compare = strcmp(WiFi.SSID().c_str(), network_ssid);
                    if (str_compare != 0 && this->Disconnect())
                    {
                        this->Connect(network_ssid, network_password) ? request->send(200, "text/plain", "Ok") : request->send(200, "text/plain", "Failed to Connect");
                        return;
                    }
                    else if (str_compare == 0)
                    {
                        request->send(200, "text/plain", "Already Connected");
                    }
                }
            }
        });
    async_server.begin();
}

void SoftAP::SetupSoftAP()
{
    // Initialization of SoftAP to be able to connect to specified WiFi network
    WiFi.disconnect();
    WiFi.softAP(SoftAP_ssid, SoftAP_password);

    Serial.print("SoftAP IP address = ");
    IPAddress softAPIP = WiFi.softAPIP();
    Serial.println(softAPIP);

    SetupMDNS("EWstation");
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

    // uncomment this if needed
    if (WiFi.isConnected())
    {
        String current_ssid = WiFi.SSID();
        htmlContent.replace("<br />", "You're already connected to: " + current_ssid);
        htmlContent.replace("display: none;", "");
        htmlContent.replace("ew_id", "EW-" + String(ESP.getEfuseMac()));
    }

    // xz kodel cia perkelt reikejo
    file.close();

    // Send modified content as response
    request->send(200, "text/html", htmlContent);
}

/// @brief Check if connection is established with network
/// @param request
void SoftAP::CheckConnectionRequest(AsyncWebServerRequest *request)
{
    if (WiFi.isConnected())
    {
        request->send(200, "text/plain", "Ok");
    }
    else
    {
        request->send(200, "text/plain", "Not Connected");
    }
}

/// @brief Search for WiFi networks
void SoftAP::SearchForAPsRequest(AsyncWebServerRequest *request)
{
    Serial.begin(115200);
    int number_of_networks = -1;

    while (true)
    {
        long currentMillis = millis();
        // trigger Wi-Fi network scan
        if (currentMillis - lastScanMillis > SCAN_PERIOD)
        {
            WiFi.scanNetworks(true);
            Serial.print("\nScan start ... ");
            lastScanMillis = currentMillis;
        }

        // print out Wi-Fi network scan result upon completion
        number_of_networks = WiFi.scanComplete();

        if (number_of_networks >= 0)
        {
            break;
        }
    }

    if (number_of_networks < 0)
    {
        request->send(200, "text/plain", "Could not find any networks.");
        return;
    }
    else if (number_of_networks > MAX_NETWORKS_SHOWN)
    {
        number_of_networks = MAX_NETWORKS_SHOWN;
    }

    String jsonString = "";
    JsonDocument doc;
    JsonObject object = doc.to<JsonObject>();
    JsonArray networks = object["networks"].to<JsonArray>();
    JsonArray signals_strength = object["signals_strength"].to<JsonArray>();

    for (int i = 0; i < number_of_networks; ++i)
    {
        networks.add(WiFi.SSID(i));
        signals_strength.add(WiFi.RSSI(i));
    }

    WiFi.scanDelete();
    convertFromJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
}

/// @brief Connects to WiFi network
bool SoftAP::Connect(String network_ssid, String network_password)
{
    Serial.begin(115200);

    WiFi.setAutoReconnect(true);
    WiFi.begin(network_ssid, network_password);

    int connectionAttempts = 0;
    try
    {
        while (WiFi.status() != WL_CONNECTED && connectionAttempts < MAX_CONNECTION_ATTEMPTS)
        {
            connectionAttempts++;
            delay(500);
        }
    }
    catch (const std::exception &e)
    {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    SaveConnectionData(network_ssid, network_password);

    Serial.println(WiFi.localIP());

    return true;
}

/// @brief Disconnects from WiFi network
bool SoftAP::Disconnect()
{
    return WiFi.disconnect();
}

/// @brief will force a disconnect and then start reconnecting to AP
/// @return true when successful
bool SoftAP::Reconnect()
{
    return WiFi.reconnect();
}

/// @brief Setup mdns name
/// @param mdns_name
void SoftAP::SetupMDNS(String mdns_name)
{
    if (!MDNS.begin(mdns_name))
    {
        Serial.println("Error setting up mDNS");
        ESP.restart();
    }
    else
    {
        Serial.println("mDNS established successfully");
    }

    MDNS.addService("http", "tcp", 80);
}

/// @brief Save connection data to eeprom
/// @param ssid network ssid
/// @param password network password
void SoftAP::SaveConnectionData(String ssid, String password)
{
    // Write SSID, password, and connection flag to EEPROM
    EEPROM.put(EEPROM_SSID_ADDRESS, ssid);
    EEPROM.put(EEPROM_PASSWORD_ADDRESS, password);
    EEPROM.write(EEPROM_CONNECTION_FLAG_ADDRESS, 1);
    EEPROM.commit();
}

/// @brief Sets up device name and saves it in EEPROM
String SoftAP::GetDeviceName()
{
    String device_name;
    byte device_name_flag = EEPROM.read(EEPROM_DEVICE_NAME_FLAG_ADDRESS);

    if (device_name_flag == 1)
    {
        Serial.println("Previous device name detected.");
        EEPROM.get(EEPROM_DEVICE_NAME_ADDRESS, device_name);
    }
    else
    {
        Serial.println("Previous device name not detected.");
        device_name = "EW-" + String(ESP.getEfuseMac());
        EEPROM.put(EEPROM_DEVICE_NAME_ADDRESS, device_name);
        EEPROM.put(EEPROM_DEVICE_NAME_FLAG_ADDRESS, 1);
        EEPROM.commit();
    }

    return device_name;
}