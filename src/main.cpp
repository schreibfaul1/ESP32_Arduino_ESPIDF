#include "Arduino.h"
#include "WiFiMulti.h"
#include "Audio.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26
#define SD_MMC_D0      2
#define SD_MMC_CLK    14
#define SD_MMC_CMD    15
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define I2S_DOUT      9
#define I2S_BCLK      3
#define I2S_LRC       1
#define SD_MMC_D0    11
#define SD_MMC_CLK   13
#define SD_MMC_CMD   14
#endif

#ifdef CONFIG_IDF_TARGET_ESP32P4
#define I2S_DOUT      23
#define I2S_BCLK      24
#define I2S_LRC       25
#define SD_MMC_D0     39
#define SD_MMC_CLK    43
#define SD_MMC_CMD    44
#endif

Audio audio;
WiFiMulti wifiMulti;

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
    wifiMulti.addAP(WIFI_SSID, WIFI_PASS);
    wifiMulti.run(); // if there are multiple access points, use the strongest one
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    pinMode(SD_MMC_D0, INPUT_PULLUP);
    // SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    // SD_MMC.begin("/sdcard", true);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(12); // default 0...21
  //  audio.connecttoFS(SD_MMC, "/test.mp3");
  //  audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/"); // aac
  //  audio.connecttohost("http://stream.danubiusradio.hu:8091/danubius_HiFi"); // flac
    audio.connecttohost("http://bcast.vigormultimedia.com:8888/sjcomplflac");

}

void loop() {
    audio.loop();
    vTaskDelay(1);
}

