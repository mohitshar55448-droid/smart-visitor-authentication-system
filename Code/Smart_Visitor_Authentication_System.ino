#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include "esp_http_server.h"

// ======================================
// WIFI DETAILS
// ======================================
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// ======================================
// TELEGRAM DETAILS
// ======================================
String BOTtoken = "YOUR_BOT_TOKEN";
String CHAT_ID = "YOUR_CHAT_ID";

// ======================================
// CAMERA MODEL AI THINKER
// ======================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ======================================
// FLASH LED
// ======================================
#define FLASH_LED 4

// ======================================
// PUSH BUTTON
// ======================================
#define BUTTON_PIN 13

// ======================================
// SERVO
// ======================================
#define SERVO_PIN 12

Servo myServo;

// ======================================
// TELEGRAM BOT
// ======================================
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

unsigned long lastTimeBotRan;
const int botRequestDelay = 1000;

String camera_url;

// ======================================
// CAMERA SERVER
// ======================================
httpd_handle_t stream_httpd = NULL;

static esp_err_t capture_handler(httpd_req_t *req) {

  camera_fb_t * fb = NULL;

  fb = esp_camera_fb_get();

  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");

  httpd_resp_send(
    req,
    (const char *)fb->buf,
    fb->len
  );

  esp_camera_fb_return(fb);

  return ESP_OK;
}

void startCameraServer() {

  httpd_config_t config =
  HTTPD_DEFAULT_CONFIG();

  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(
      &stream_httpd,
      &config) == ESP_OK) {

    httpd_register_uri_handler(
      stream_httpd,
      &capture_uri
    );
  }
}

// ======================================
// CAMERA INIT
// ======================================
void startCamera() {

  camera_config_t config;

  config.ledc_channel =
  LEDC_CHANNEL_0;

  config.ledc_timer =
  LEDC_TIMER_0;

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

  config.pin_sscb_sda =
  SIOD_GPIO_NUM;

  config.pin_sscb_scl =
  SIOC_GPIO_NUM;

  config.pin_pwdn =
  PWDN_GPIO_NUM;

  config.pin_reset =
  RESET_GPIO_NUM;

  config.xclk_freq_hz =
  20000000;

  config.pixel_format =
  PIXFORMAT_JPEG;

  if (psramFound()) {

    config.frame_size =
    FRAMESIZE_VGA;

    config.jpeg_quality = 10;

    config.fb_count = 2;

  } else {

    config.frame_size =
    FRAMESIZE_CIF;

    config.jpeg_quality = 12;

    config.fb_count = 1;
  }

  esp_err_t err =
  esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.println(
    "Camera Init Failed");

    return;
  }

  Serial.println("Camera Ready");
}

// ======================================
// HANDLE TELEGRAM COMMANDS
// ======================================
void handleNewMessages(
int numNewMessages) {

  for (int i = 0;
  i < numNewMessages;
  i++) {

    String text =
    bot.messages[i].text;

    String chat_id =
    bot.messages[i].chat_id;

    // SECURITY CHECK
    if (chat_id != CHAT_ID) {

      bot.sendMessage(
      chat_id,
      "Unauthorized User",
      "");

      continue;
    }

    // START
    if (text == "/start") {

      String welcome =
      "SMART SECURITY SYSTEM ACTIVE\n\n";
      welcome +=
      "Waiting For Visitor...";

      bot.sendMessage(
      CHAT_ID,
      welcome,
      "");
    }

    // NORMAL CAPTURE
    if (text == "capture") {

      bot.sendMessage(
      CHAT_ID,
      "Normal Image:\n" +
      camera_url,
      "");

      bot.sendMessage(
      CHAT_ID,
      "Allow or Deny?",
      "");
    }

    // FLASH CAPTURE
    if (text == "flashcapture") {

      digitalWrite(
      FLASH_LED,
      HIGH);

      delay(500);

      bot.sendMessage(
      CHAT_ID,
      "Flash Image:\n" +
      camera_url,
      "");

      bot.sendMessage(
      CHAT_ID,
      "Allow or Deny?",
      "");

      delay(1000);

      digitalWrite(
      FLASH_LED,
      LOW);
    }

    // ALLOW
    if (text == "allow") {

      bot.sendMessage(
      CHAT_ID,
      "Access Granted",
      "");

      myServo.write(90);

      delay(5000);

      myServo.write(0);

      bot.sendMessage(
      CHAT_ID,
      "Gate Closed",
      "");
    }

    // DENY
    if (text == "deny") {

      bot.sendMessage(
      CHAT_ID,
      "Access Denied",
      "");
    }
  }
}

// ======================================
// SETUP
// ======================================
void setup() {

  Serial.begin(115200);

  // FLASH LED
  pinMode(FLASH_LED, OUTPUT);

  digitalWrite(
  FLASH_LED,
  LOW);

  // PUSH BUTTON
  pinMode(
  BUTTON_PIN,
  INPUT_PULLUP);

  // SERVO
  myServo.attach(SERVO_PIN);

  myServo.write(0);

  // CAMERA
  startCamera();

  // WIFI
  WiFi.begin(
  ssid,
  password);

  Serial.print(
  "Connecting");

  while (WiFi.status()
  != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println("");
  Serial.println(
  "WiFi Connected");

  client.setInsecure();

  // CAMERA SERVER
  startCameraServer();

  camera_url =
  "http://" +
  WiFi.localIP().toString() +
  "/capture";

  Serial.println(camera_url);

  bot.sendMessage(
  CHAT_ID,
  "ESP32-CAM SECURITY SYSTEM ONLINE",
  "");
}

// ======================================
// LOOP
// ======================================
void loop() {

  // PUSH BUTTON PRESSED
  if (digitalRead(BUTTON_PIN)
      == LOW) {

    bot.sendMessage(
    CHAT_ID,
    "🚨 Somebody is at the gate!\n\nChoose:\n1. capture\n2. flashcapture",
    "");

    delay(3000);
  }

  // TELEGRAM BOT
  if (millis()
  - lastTimeBotRan
  > botRequestDelay) {

    int numNewMessages =
    bot.getUpdates(
    bot.last_message_received + 1);

    while (numNewMessages) {

      handleNewMessages(
      numNewMessages);

      numNewMessages =
      bot.getUpdates(
      bot.last_message_received + 1);
    }

    lastTimeBotRan =
    millis();
  }
}
