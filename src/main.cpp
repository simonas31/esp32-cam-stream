#define APP_CPU 1
#define PRO_CPU 0

#include "SoftAP.h"
#include "OV2640.h"
#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#include "esp_timer.h"
#include "base64.h"
#include "soc/soc.h"          //disable brownout problems
#include "soc/rtc_cntl_reg.h" //disable brownout problems

// for google cloud storage
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

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

// Google Cloud Storage bucket name and dont forget to REPLACE_WITH_YOUR_BUCKET_NAME
#define STORAGE_BUCKET_NAME "REPLACE_WITH_YOUR_BUCKET_NAME"

// Google Project ID and dont forget to REPLACE_WITH_YOUR_PROJECT_ID
#define PROJECT_ID "REPLACE_WITH_YOUR_PROJECT_ID"

// Service Account's client email and dont forget to REPLACE_WITH_YOUR_CLIENT_EMAIL
#define CLIENT_EMAIL "REPLACE_WITH_YOUR_CLIENT_EMAIL"

// Service Account's private key dont forget to REPLACE_WITH_YOUR_PRIVATE_KEY
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nREPLACE_WITH_YOUR_PRIVATE_KEY\n-----END PRIVATE KEY-----\n";

OV2640 cam;

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig fb_config;

// Define request properties
RequestProperties requestProps;

// Define soft ap object for connecting to WiFi
SoftAP *softap_client = nullptr;

bool motion_detected = false;

// Task handles for running tasks on separate cores
TaskHandle_t burstImages_t;
TaskHandle_t Task1;

const int captureInterval = 10; // Adjust for desired frame rate (ms)
const int captureTime = 5000;   // Adjust for desired capture time (ms)

// The Google Cloud Storage upload callback function
void gcsUploadCallback(UploadStatusInfo info)
{
  if (info.status == firebase_gcs_upload_status_init)
  {
    Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
  }
  else if (info.status == firebase_gcs_upload_status_upload)
  {
    Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
  }
  else if (info.status == firebase_gcs_upload_status_complete)
  {
    Serial.println("Upload completed\n");
    FileMetaInfo meta = fbdo.metaData();
    Serial.printf("Name: %s\n", meta.name.c_str());
    Serial.printf("Bucket: %s\n", meta.bucket.c_str());
    Serial.printf("contentType: %s\n", meta.contentType.c_str());
    Serial.printf("Size: %d\n", meta.size);
    Serial.printf("Generation: %lu\n", meta.generation);
    Serial.printf("ETag: %s\n", meta.etag.c_str());
    Serial.printf("CRC32: %s\n", meta.crc32.c_str());
    Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());      // only gcs_upload_type_multipart and gcs_upload_type_resumable upload types.
    Serial.printf("Download URL: %s\n", fbdo.downloadURL().c_str()); // only gcs_upload_type_multipart and gcs_upload_type_resumable upload types.
  }
  else if (info.status == firebase_gcs_upload_status_error)
  {
    Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
  }
}

void burstImages_cb(void *pvParameters)
{
  unsigned long startTime;
  String upload_filename;

  for (;;)
  {
    // uint8_t *fb = cam.getfb();
    // size_t len = cam.getSize();

    // if motion detected send images to cloud
    // if (motion_detected)
    // {
    //   startTime = millis();
    //   while (millis() - startTime < captureTime)
    //   {
    //
    // use prev code from git to implement faster and more efficient image uploading
    if (Firebase.ready())
    {
      String file_name(millis());
      File file = LittleFS.open("/temp.jpeg", FILE_WRITE);
      cam.run();
      file.write(cam.getfb(), cam.getSize());
      file.close();

      Firebase.GCStorage.upload(&fbdo, STORAGE_BUCKET_NAME, "/temp.jpeg", mem_storage_type_flash, gcs_upload_type_multipart, file_name + ".jpeg", "image/jpeg", nullptr, nullptr, nullptr, NULL);
      Serial.println("done");
      LittleFS.remove("/temp.jpeg");
    }
    //   }
    // }

    // delay(captureInterval);
  }
}

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

  // Frame size options
  // config.frame_size = FRAMESIZE_SXGA;
  // config.frame_size = FRAMESIZE_UXGA;
  // config.frame_size = FRAMESIZE_SVGA;
  // config.frame_size = FRAMESIZE_QVGA;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 15;
  config.fb_count = 2; // on esp32 psram is enabled

  if (cam.init(config) != ESP_OK)
  {
    Serial.println("Error initializing the camera.");
    delay(3000);
    ESP.restart();
  }
  Serial.println("Camera initialized successfully.");
  WiFi.begin("TP-Link_4FD6", "53503794");

  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  // core 1 executes web requests to the localhosted server
  // softap_client = new SoftAP();

  /* Assign the Service Account credentials for OAuth2.0 authen */
  fb_config.service_account.data.client_email = CLIENT_EMAIL;
  fb_config.service_account.data.project_id = PROJECT_ID;
  fb_config.service_account.data.private_key = PRIVATE_KEY;

  /* Assign the callback function for the long running token generation task */
  fb_config.token_status_callback = tokenStatusCallback;

  /* Assign upload buffer size in byte */
  // Data to be uploaded will send as multiple chunks with this size, to compromise between speed and memory used for buffering.
  // The memory from external SRAM/PSRAM will not use in the TCP client internal tx buffer.
  fb_config.gcs.upload_buffer_size = 2048;

  Firebase.reconnectNetwork(true);

  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

  // Define optional request properties
  // requestProps.contentType = "image/jpeg";

  Firebase.begin(&fb_config, &auth);

  LittleFS.begin();
  // core 0 bursts images to google cloud
  xTaskCreatePinnedToCore(
      burstImages_cb,
      "burstImages",
      15000,
      NULL,
      2,
      &burstImages_t,
      APP_CPU);
}

void loop()
{
  delay(10);
}