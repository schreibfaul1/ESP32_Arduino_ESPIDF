#include "Arduino.h"
#include "Audio.h"
#include "WiFi.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define I2S_DOUT      1
#define I2S_BCLK      2
#define I2S_LRC       3
#endif

Audio audio;

String ssid =     "*****";
String password = "*****";

void setup() {
    Serial.begin(115200);
    Serial.print("A\n\n");
    Serial.println("----------------------------------");
    Serial.printf("ESP32 Chip: %s\n", ESP.getChipModel());
    Serial.printf("Arduino Version: %d.%d.%d\n", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
    Serial.printf("ESP-IDF Version: %d.%d.%d\n", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
    Serial.printf("ARDUINO_LOOP_STACK_SIZE %d words (32 bit)\n", CONFIG_ARDUINO_LOOP_STACK_SIZE);
    Serial.println("----------------------------------");
    Serial.print("\n\n");
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21); // default 0...21
    audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/"); // aac
}

void loop() {
    audio.loop();
    vTaskDelay(1);
}

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}