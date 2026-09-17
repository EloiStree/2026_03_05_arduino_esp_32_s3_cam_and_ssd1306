
#include "esp_camera.h"
#include <WiFi.h>

// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

// ===========================
// Enter your WiFi credentials
// ===========================

const char *ssid_first_attempt = "SSD1306_8";
const char *ssid_backup_attempt = "SSD1306";

const char *password = "12345678";

// ===========================
// Function declarations
// ===========================

void startCameraServer();
void setupLedFlash();

void setup() {

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // ===========================
  // Camera configuration
  // ===========================

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

  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;

  // config.pixel_format = PIXFORMAT_RGB565;
  // for face detection/recognition

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // ===========================
  // Configure camera depending
  // on PSRAM availability
  // ===========================

  if (config.pixel_format == PIXFORMAT_JPEG) {

    if (psramFound()) {

      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;

    } else {

      // Limit frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }

  } else {

    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;

#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif

  }

  // ===========================
  // ESP-EYE configuration
  // ===========================

#if defined(CAMERA_MODEL_ESP_EYE)

  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);

#endif

  // ===========================
  // Initialize camera
  // ===========================

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.printf(
      "Camera init failed with error 0x%x",
      err
    );

    return;
  }

  sensor_t *s = esp_camera_sensor_get();

  // ===========================
  // Sensor configuration
  // ===========================

  if (s->id.PID == OV3660_PID) {

    // Flip image back
    s->set_vflip(s, 1);

    // Increase brightness slightly
    s->set_brightness(s, 1);

    // Reduce saturation
    s->set_saturation(s, -2);
  }

  // ===========================
  // Set initial frame size
  // ===========================

  if (config.pixel_format == PIXFORMAT_JPEG) {

    s->set_framesize(s, FRAMESIZE_QVGA);
  }

  // ===========================
  // M5Stack camera configuration
  // ===========================

#if defined(CAMERA_MODEL_M5STACK_WIDE) || \
    defined(CAMERA_MODEL_M5STACK_ESP32CAM)

  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);

#endif

  // ===========================
  // ESP32-S3-EYE configuration
  // ===========================

#if defined(CAMERA_MODEL_ESP32S3_EYE)

  s->set_vflip(s, 1);

#endif

  // ===========================
  // Setup LED flash
  // ===========================

#if defined(LED_GPIO_NUM)

  setupLedFlash();

#endif

  // =====================================================
  // WiFi connection
  // =====================================================

  WiFi.setSleep(false);

  Serial.println();
  Serial.println("==============================");
  Serial.println("WiFi connection");
  Serial.println("==============================");

  // -----------------------------------------------------
  // Try first WiFi network
  // -----------------------------------------------------

  Serial.print("Trying first WiFi: ");
  Serial.println(ssid_first_attempt);

  WiFi.begin(ssid_first_attempt, password);

  unsigned long startAttemptTime = millis();

  // Wait up to 10 seconds
  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startAttemptTime < 10000
  ) {

    delay(500);
    Serial.print(".");
  }

  // -----------------------------------------------------
  // If first WiFi failed, try backup WiFi
  // -----------------------------------------------------

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println();
    Serial.println("First WiFi connection failed.");

    // Disconnect from first network
    WiFi.disconnect(true);

    delay(500);

    Serial.print("Trying backup WiFi: ");
    Serial.println(ssid_backup_attempt);

    WiFi.begin(ssid_backup_attempt, password);

    startAttemptTime = millis();

    // Wait up to 10 seconds
    while (
      WiFi.status() != WL_CONNECTED &&
      millis() - startAttemptTime < 10000
    ) {

      delay(500);
      Serial.print(".");
    }
  }

  // =====================================================
  // Check WiFi connection
  // =====================================================

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("==============================");
    Serial.println("WiFi connected!");
    Serial.println("==============================");

    Serial.print("Network: ");
    Serial.println(WiFi.SSID());

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

  } else {

    Serial.println();
    Serial.println("==============================");
    Serial.println("WiFi connection failed!");
    Serial.println("==============================");

    Serial.println("Neither WiFi network could be found.");

    // Stop setup here
    return;
  }

  // =====================================================
  // Start camera web server
  // =====================================================

  startCameraServer();

  Serial.println();
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

// =====================================================
// Main loop
// =====================================================

void loop() {

  // Everything is handled by the camera web server
  delay(10000);
}
