#include "Arduino.h"
#include "Audio.h"
#include "WiFi.h"

#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

Audio audio;

String ssid =     "Wolles-FRITZBOX";
String password = "40441061073895958449";

void setup() {
    Serial.begin(115200);
    Serial.print("A\n\n");
    Serial.println("----------------------------------");
    uint8_t avMajor  = ESP_ARDUINO_VERSION_MAJOR;
    uint8_t avMinor  = ESP_ARDUINO_VERSION_MINOR;
    uint8_t avPatch  = ESP_ARDUINO_VERSION_PATCH;
    Serial.printf("ESP32 Chip: %s\n", ESP.getChipModel());
    Serial.printf("Arduino Version: %d.%d.%d\n", avMajor, avMinor, avPatch);
    uint8_t idfMajor = ESP_IDF_VERSION_MAJOR;
    uint8_t idfMinor = ESP_IDF_VERSION_MINOR;
    uint8_t idfPatch = ESP_IDF_VERSION_PATCH;
    Serial.printf("ESP-IDF Version: %d.%d.%d\n", idfMajor, idfMinor, idfPatch);
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