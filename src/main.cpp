#include "SoftAP.h"
#include <WebSocketsServer.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h"          //disable brownout problems
#include "soc/rtc_cntl_reg.h" //disable brownout problems
#include "driver/gpio.h"

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

const char *ssid = "";                            // change to your router ssid
const char *password = "";                        // change to your router password
const char *server_url = "http://127.0.0.0:8080"; // server url where fall detection is being performed and the server where people want to watch this camera

const char *websockets_server_host = "192.168.1.149"; // CHANGE HERE
const uint16_t websockets_server_port = 3001;         // OPTIONAL CHANGE

WebSocketsServer ws(81);

camera_fb_t *fb = NULL;
size_t _jpg_buf_len = 0;
uint8_t *_jpg_buf = NULL;
uint8_t state = 0;
SoftAP *client = nullptr;

TaskHandle_t Task0;
TaskHandle_t Task1;

// void notifyClients(const char *buffer, size_t buf_len)
// {
//   ws.(buffer, buf_len);
// }

void webSocketEvent(byte num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED: // enum that read status this is used for debugging.
    Serial.print("WS Type ");
    Serial.print(type);
    Serial.println(": DISCONNECTED");
    break;
  case WStype_CONNECTED: // Check if a WebSocket client is connected or not
    Serial.print("WS Type ");
    Serial.print(type);
    Serial.println(": CONNECTED");
    break;
  case WStype_TEXT: // check response from client
    Serial.println((char *)payload);
    // Check if the message is "getStream"
    if (strcmp((char *)payload, "getStream") == 0)
    {
      fb = esp_camera_fb_get();
      if (!fb)
      {
        Serial.println("img capture failed");
        esp_camera_fb_return(fb);
        ESP.restart();
      }
      ws.broadcastBIN(fb->buf, fb->len);
      Serial.println("image sent");
      esp_camera_fb_return(fb);
    }
    break;
  }
}

esp_err_t initCamera()
{
  Serial.begin(115200);
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

  if (psramFound())
  {
    config.frame_size = FRAMESIZE_VGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 15;          // 10-63 lower number means higher quality
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.println("bad camera init");
    return err;
  }
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);
  Serial.println("camera init OK");
  return ESP_OK;
};

// esp_err_t initWiFi()
// {
//   WiFi.begin(ssid, password);
//   Serial.println("Wifi init ");

//   while (WiFi.status() != WL_CONNECTED)
//   {
//     delay(500);
//     Serial.print(".");
//   }

//   Serial.println("");
//   Serial.println("WiFi OK");
//   Serial.println(WiFi.localIP());
//   return ESP_OK;
// };

esp_err_t initWebSocketsServer()
{
  Serial.println("WebSocket server init");
  ws.begin();
  ws.onEvent(webSocketEvent);
  Serial.println("WebSocket server ok");
  return ESP_OK;
}

void SendDataToWS_Server(void *pvParameters)
{
  Serial.begin(115200);
  initCamera();
  initWebSocketsServer();

  for (;;)
  {
    ws.loop();
    if (ws.connectedClients() > 0 && WiFi.isConnected())
    {
      fb = esp_camera_fb_get();
      if (!fb)
      {
        Serial.println("img capture failed");
        esp_camera_fb_return(fb);
        ESP.restart();
      }
      ws.broadcastBIN(fb->buf, fb->len);
      Serial.println("image sent");
      esp_camera_fb_return(fb);
    }
    // Add a small delay to let the watchdog process
    // https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time
    delay(10);
  }
}

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

  Serial.begin(115200);
  Serial.setDebugOutput(false);

  // core 0 executes sending images to the server
  xTaskCreatePinnedToCore(SendDataToWS_Server, "Sending images to WS", 10000, NULL, 1, &Task0, 0);

  // core 1 executes web requests to the localhosted server
  client = new SoftAP();
  // initCamera();
  // initWiFi();
  // initWebSocketsServer();
}

void loop()
{
  // restore connection here???
  delay(10);
}