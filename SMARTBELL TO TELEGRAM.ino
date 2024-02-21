#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Wire.h>

// Isi dengan alamat dan password WiFi kalian
const char* ssid = "*************";
const char* password = "***************";

String BOTtoken = "*************:***********************************"; // Ganti dengan token dari BotFather Telegram
String chatId = "*******************";     // Ganti dengan ID chat dari bot

bool sendPhoto = false;
bool buttonState = false;
bool previousButtonState = false;
int buzzerPin = 2; // Pin buzzer yang terhubung

WiFiClientSecure clientTCP;

UniversalTelegramBot bot(BOTtoken, clientTCP);

// CAMERA_MODEL_AI_THINKER
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

#define FLASHpin 4
bool flashState = LOW;
bool adaGerakan = false;
int botRequestDelay = 1000;   // Setiap 1 detik akan cek bot
long lastTimeBotRan;    

void handleNewMessages(int numNewMessages);
String sendPhotoTelegram();

static void IRAM_ATTR detectsMovement(void* arg){
  adaGerakan = true;
}

void setup(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
  
  pinMode(FLASHpin, OUTPUT);
  digitalWrite(FLASHpin, flashState);

  pinMode(4, INPUT_PULLUP); // Set GPIO 4 sebagai input dengan pull-up resistor
  
  pinMode(buzzerPin, OUTPUT);
  
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Menghubungkan WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  clientTCP.setInsecure(); // Gunakan certificate root bawaan dari library WiFiClientSecure.h
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Inisialisasi dengan spesifikasi tinggi untuk mengalokasikan buffer yang lebih besar
  if (psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  // 0-63, semakin rendah semakin tinggi kualitas
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  // 0-63, semakin rendah semakin tinggi kualitas
    config.fb_count = 1;
  }
  
  // Inisialisasi kamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Inisialisasi kamera gagal dengan kode error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // Menurunkan ukuran frame untuk meningkatkan frame rate awal
  sensor_t* s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_CIF);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA

  // Mode PIR Motion Sensor INPUT_PULLUP
  err = gpio_isr_handler_add(GPIO_NUM_13, &detectsMovement, (void*) 13);  // Pin sensor yang digunakan
  if (err != ESP_OK){
    Serial.printf("Penambahan handler gagal dengan kode error 0x%x \r\n", err); 
  }
  err = gpio_set_intr_type(GPIO_NUM_13, GPIO_INTR_POSEDGE);
  if (err != ESP_OK){
    Serial.printf("Penetapan tipe intrupsi gagal dengan kode error 0x%x \r\n", err);
  }
}

void loop(){
  buttonState = digitalRead(4);  // Baca status tombol pada GPIO 4

  if (buttonState != previousButtonState){
    if (buttonState == LOW){
      sendPhoto = true;
      Serial.println("Permintaan foto baru");
    }
    else {
       digitalWrite(buzzerPin, LOW); // Matikan buzzer saat tombol dilepas
    }
    delay(50);
  }

  previousButtonState = buttonState;

  if (sendPhoto){
    Serial.println("Persiapan mengirim foto");
    sendPhotoTelegram(); 
    sendPhoto = false; 
  }

  if (adaGerakan){
    bot.sendMessage(chatId, "ADA GERAKAN !!!", "");
    Serial.println("Ada gerakan");
    sendPhotoTelegram();
    adaGerakan = false;
  }
  
  if (millis() > lastTimeBotRan + botRequestDelay){
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages){
      Serial.println("Mendapatkan respons");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

String sendPhotoTelegram(){
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t* fb = NULL;
  fb = esp_camera_fb_get();  
  if (!fb) {
    Serial.println("Kamera gagal mengambil gambar");
    delay(1000);
    ESP.restart();
    return "Kamera gagal mengambil gambar";
  }  
  
  Serial.println("Terhubung ke " + String(myDomain));

  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Berhasil terhubung");
    
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + chatId + "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot" + BOTtoken + "/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t* fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n = n + 1024) {
      if (n + 1024 < fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      } else if (fbLen % 1024 > 0) {
        size_t remainder = fbLen % 1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // Timeout 10 detik
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state == true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length() == 0) state = true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length() > 0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody = "Koneksi ke api.telegram.org gagal.";
    Serial.println("Koneksi ke api.telegram.org gagal.");
  }
  return getBody;
}

void handleNewMessages(int numNewMessages){
  Serial.print("Menangani Pesan Baru: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++){
    // ID chat pengirim
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != chatId){
      bot.sendMessage(chat_id, "Pengguna tidak diotorisasi", "");
      continue;
    }
    
    // Cetak pesan yang diterima
    String text = bot.messages[i].text;
    Serial.println(text);

    String fromName = bot.messages[i].from_name;

    if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASHpin, flashState);
    }

    if (text == "/photo") {
      sendPhoto = true;
    }

    if (text == "/start") {
      String welcome = "Selamat datang, " + fromName + "!\n"
                       "Ketik /flash untuk mengontrol LED flash.\n"
                       "Ketik /photo untuk mengambil foto.";
      bot.sendMessage(chatId, welcome, "");
    }
  }
}