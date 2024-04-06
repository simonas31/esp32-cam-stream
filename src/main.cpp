#define APP_CPU 1
#define PRO_CPU 0

#include "SoftAP.h"
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <esp_camera.h>
#include <driver/rtc_io.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "esp_timer.h"
#include "base64.h"
#include "soc/soc.h"          //disable brownout problems
#include "soc/rtc_cntl_reg.h" //disable brownout problems

// configuration for AI Thinker Camera board
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Define soft ap object for connecting to WiFi
SoftAP *softap_client = nullptr;

bool motion_detected = false;

// Task handles for running tasks on separate cores
TaskHandle_t burstImages_t;
TaskHandle_t Task1;

// MQTT Broker settings
const char *mqtt_broker = "bc316ac1.emqx.cloud";
const char *mqtt_topic = "ews_dev";
const char *mqtt_username = "ews_dev";
const char *mqtt_password = "ews_dev";
const int mqtt_port = 1883;

// WiFi and MQTT client initialization
WiFiClient esp_client;
PubSubClient mqtt_client(esp_client);

camera_fb_t *fb = NULL;
size_t _jpg_buf_len = 0;
uint8_t *_jpg_buf = NULL;

bool sensorTriggered = false;          // Flag to track if sensor has been triggered
unsigned long lastTriggerTime = 0;     // Time of last trigger
const unsigned long delayTime = 60000; // Delay time in milliseconds (60 seconds)
const int PIR_SENSOR_PIN = 13;         // Pin connected to PIR sensor

void connectToMQTT()
{
  while (!mqtt_client.connected())
  {
    String client_id = "esp32-client-" + String(WiFi.macAddress());
    Serial.printf("Connecting to MQTT Broker as %s...\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("Connected to MQTT broker");
      // mqtt_client.subscribe(mqtt_topic);
      // mqtt_client.publish(mqtt_topic, "Hi EMQX I'm ESP32 ^^"); // Publish message upon connection
    }
    else
    {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" Retrying in 5 seconds.");
      delay(5000);
    }
  }
}

void sendPayload(String encodedImage, String jsonString, JsonDocument doc, int &current_image_number, bool last_image)
{
  if (!mqtt_client.connected())
  {
    connectToMQTT();
  }
  mqtt_client.loop();
  fb = esp_camera_fb_get();

  if (!fb)
  {
    Serial.println("img capture failed");
    esp_camera_fb_return(fb);
    ESP.restart();
  }

  // create json object with data
  encodedImage = base64::encode(fb->buf, fb->len);
  doc["image"] = "\"" + encodedImage + "\"";
  doc["file_name"] = "\"EW" + String(ESP.getEfuseMac()) + "\"";
  doc["image_number"] = current_image_number;
  // find out if last message
  if (last_image)
  {
    doc["last_image"] = true;
  }
  else
  {
    doc["last_image"] = false;
  }
  current_image_number++;

  convertFromJson(doc, jsonString);
  mqtt_client.publish(mqtt_topic, jsonString.c_str());
  esp_camera_fb_return(fb);
}

void burstImages_cb(void *pvParameters)
{
  int current_image_number = 1;
  Serial.println("Starting upload...");
  String encodedImage, jsonString = "";
  JsonDocument doc;
  long int currentTime = millis();

  while (!WiFi.isConnected())
  {
    Serial.print("Process needs to be connected to wifi");
    delay(500);
  }

  Serial.print("Process connected to wifi");

  for (;;)
  {
    if (sensorTriggered && WiFi.isConnected())
    {
      Serial.println("Begin uploading...");
      currentTime = millis();
      while (millis() - currentTime <= 15000)
      {
        sendPayload(encodedImage, jsonString, doc, current_image_number, false);
      }
      // add last frame send that it is the last and start creating video in python
      delay(100);
      sendPayload(encodedImage, jsonString, doc, current_image_number, true);
      Serial.println("Done uploading...");
    }

    delay(10);
  }
}

esp_err_t initCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // parameters for image quality and size
  config.frame_size = FRAMESIZE_XGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
  config.jpeg_quality = 15;          // 10-63 lower number means higher quality
  config.fb_count = 2;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("camera init FAIL: 0x%x", err);
    delay(2000);
    ESP.restart();
  }
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);
  Serial.println("camera init OK");
  return ESP_OK;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  initCamera();

  Serial.println("Camera initialized successfully.");
  // core 1 executes web requests to the localhosted server
  softap_client = new SoftAP();

  // Set Root CA certificate
  // esp_client.setCACert(ca_cert);
  // esp_client.setInsecure();

  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setKeepAlive(60); // the default is 60 seconds, and it is disabled when it is set to 0
  mqtt_client.setBufferSize(30000);

  // core 0 bursts images to google cloud
  // xTaskCreatePinnedToCore(
  //     burstImages_cb,
  //     "burstImages",
  //     15000,
  //     NULL,
  //     2,
  //     &burstImages_t,
  //     APP_CPU);

  pinMode(PIR_SENSOR_PIN, INPUT);
}

void loop()
{
  if (WiFi.isConnected())
  {
    int sensorValue = digitalRead(PIR_SENSOR_PIN);

    if (sensorValue == HIGH && !sensorTriggered)
    {
      // Sensor is triggered
      Serial.println("Sensor Triggered!");
      sensorTriggered = true;
      lastTriggerTime = millis();
      // digitalWrite(LED_PIN, HIGH); // Turn on LED
    }

    if (sensorTriggered && (millis() - lastTriggerTime >= delayTime))
    {
      // Delay time has passed
      Serial.println("Sensor can now be triggered again.");
      sensorTriggered = false;
      // digitalWrite(LED_PIN, LOW); // Turn off LED
    }
  }
  delay(10);
}