#include "Arduino.h"
#include "Audio.h"
#include "WiFiMulti.h"
#include "esp_psram.h"
#include "pins_arduino.h"

#ifdef CONFIG_IDF_TARGET_ESP32
    #define I2S_DOUT_PIN   25
    #define I2S_BCLK_PIN   27
    #define I2S_LRC_PIN    26
    #define SD_MMC_D0_PIN  2
    #define SD_MMC_CLK_PIN 14
    #define SD_MMC_CMD_PIN 15
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
    #define I2S_DOUT_PIN   9
    #define I2S_BCLK_PIN   3
    #define I2S_LRC_PIN    1
    #define SD_MMC_D0_PIN  11
    #define SD_MMC_CLK_PIN 13
    #define SD_MMC_CMD_PIN 14
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S31
    #define I2S_DOUT_PIN   44
    #define I2S_BCLK_PIN   45
    #define I2S_LRC_PIN    46
    #define SD_MMC_D0_PIN  47
    #define SD_MMC_CLK_PIN 48
    #define SD_MMC_CMD_PIN 49
#endif

#ifdef CONFIG_IDF_TARGET_ESP32P4
    #define I2S_DOUT_PIN   23
    #define I2S_BCLK_PIN   24
    #define I2S_LRC_PIN    25
    #define SD_MMC_D0_PIN  39
    #define SD_MMC_CLK_PIN 43
    #define SD_MMC_CMD_PIN 44
#endif



Audio     audio;
WiFiMulti wifiMulti;
uint32_t  t;

void my_audio_info(Audio::msg_t m) {
    printf("%s: %s\n", m.s, m.msg);
}

void setup() {
    vTaskDelay(5000);
    Audio::audio_info_callback = my_audio_info;
    printf("\n\n");
    printf("----------------------------------\n");
    printf("ESP32 Chip: %s\n", ESP.getChipModel());
    printf("Arduino Version: %d.%d.%d\n", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
    printf("ESP-IDF Version: %d.%d.%d\n", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
    printf("ARDUINO_LOOP_STACK_SIZE %d words (32 bit)\n", CONFIG_ARDUINO_LOOP_STACK_SIZE);
    printf("audioI2S Version: %s\n", audio.getVersion());
    printf("----------------------------------\n");
    printf("PSRAM size: %i\n", esp_psram_get_size());
    printf("PSRAM free: %i\n", ESP.getFreePsram());
    printf("\n\n");
    wifiMulti.addAP(WIFI_SSID, WIFI_PASS);
    wifiMulti.run(); // if there are multiple access points, use the strongest one
    while (WiFi.status() != WL_CONNECTED) delay(1500);
    pinMode(SD_MMC_D0_PIN, INPUT_PULLUP);
//    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
//    SD_MMC.begin("/sdcard", true);
    // audio.settings.SPECTRUM = true;
    audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
    audio.setVolume(12); // default 0...21
    audio.settings.SPECTRUM=false;

    //    audio.connecttohost("http://4c4b867c89244861ac216426883d1ad0.msvdn.net/radiom2o/radiom2o/master_ma.m3u8");
    // audio.connecttohost("https://s1.knixx.fm/dein_webradio_512.opus?_=n3omm7iqj6dctcryodwuwv");
    // audio.connecttohost("http://amp1.cesnet.cz:8000/cro2-256.ogg");
    // audio.connecttohost("http://icecast.radiofrance.fr/franceinfo-lofi.aac");
        audio.connecttohost("https://streaming.nrjaudio.fm/oua8a3w2dqao?origine=playernostalgie&aw_0_req.userConsentV2=&aw_0_1st.station=");
    // audio.connecttohost("https://air.pc.cdn.bitgravity.com/air/live/pbaudio001/playlist.m3u8");
    // audio.connecttohost("https://rfienchinois64k.ice.infomaniak.ch/rfienchinois-64.mp3");
    // audio.connecttohost("http://icecast6.play.cz/radio-xaver.aac");
    // audio.connecttohost("http://0n-70s.radionetz.de/0n-70s.mp3");

    audio.settings.BT_DEV_NANE = " Pebble V3";
    audio.settings.BT_SOURCE=true;
}

void loop() {
    audio.loop();
    vTaskDelay(1);
    // if (t < millis()) {
    //     t = millis() + 4000;
    //     printf("free heap %i, PSRAM free %iInbuff filled %i\n", ESP.getFreeHeap(), ESP.getFreePsram(), audio.inBufferFilled());
    //     audio.samplesBufferStatus();
    // }
}