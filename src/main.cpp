#define APP_CPU 1
#define PRO_CPU 0

#include "SoftAP.h"
#include "OV2640.h"
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#include "esp_timer.h"
#include "img_converters.h"
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

OV2640 cam;

SoftAP *softap_client = nullptr;

TaskHandle_t Task0;
TaskHandle_t Task1;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Configure the camera
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

  // Frame parameters: pick one
  // config.frame_size = FRAMESIZE_SXGA;
  // config.frame_size = FRAMESIZE_UXGA;
  // config.frame_size = FRAMESIZE_SVGA;
  // config.frame_size = FRAMESIZE_QVGA;
  config.frame_size = FRAMESIZE_SXGA;
  config.jpeg_quality = 15;
  config.fb_count = 2;

  if (cam.init(config) != ESP_OK)
  {
    Serial.println("Error initializing the camera");
    delay(3000);
    ESP.restart();
  }

  // core 1 executes web requests to the localhosted server
  softap_client = new SoftAP();

  // core 0 bursts images to google cloud
  // xTaskCreatePinnedToCore(
  //     mjpegCB,
  //     "mjpeg",
  //     4 * 1024,
  //     NULL,
  //     2,
  //     &tMjpeg,
  //     APP_CPU);
}

void loop()
{
  // restore connection here???
  delay(10);
}