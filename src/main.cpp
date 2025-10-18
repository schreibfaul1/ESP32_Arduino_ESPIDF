#include "Arduino.h"
#include "Audio.h"
#include "WiFi.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define I2S_DOUT      9
#define I2S_BCLK      3
#define I2S_LRC       1
#endif

#ifdef CONFIG_IDF_TARGET_ESP32P4
#define I2S_DOUT      22
#define I2S_BCLK      20
#define I2S_LRC       21
#endif

String ssid =     "Wolles-FRITZBOX";
String password = "40441061073895958449";

Audio audio;

void my_audio_info(Audio::msg_t m) {
    Serial.printf("%s: %s\n", m.s, m.msg);
}

void setup() {
    Serial.begin(115200);
    Audio::audio_info_callback = my_audio_info;
    Serial.print("\n\n");
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
    audio.setVolume(20); // default 0...21
  //  audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/"); // aac
    audio.connecttohost("http://stream.danubiusradio.hu:8091/danubius_HiFi"); // flac
  //  audio.connecttohost("http://stream.revma.ihrhls.com/zc4882/hls.m3u8");

}

void loop() {
    audio.loop();
    vTaskDelay(1);
}

