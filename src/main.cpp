
/* Program to update the FW of the WiFI chip ESP32C6 on an ESP32-P4 board

1) Select and download the appropriate FW version from: https://esphome.github.io/esp-hosted-firmware/manifest/esp32c6.json

2) Paste the downloaded file into the "/data" folder

3) Build + Upload

4) goto PIOARDUINO: "Upload Filesystem Image"

5) Reset

to erase everything close the serial terminal and go to PIOARDUINO: "Erase Flash"

*/

#include "Arduino.h"
#include "FS.h"
#include "LittleFS.h"
#include "dirent.h"
#include "esp_app_desc.h"
#include "esp_hosted.h"
#include "esp_hosted_api_types.h"
#include "esp_hosted_ota.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "ota_littlefs/ota_littlefs.h"

esp_hosted_coprocessor_fwver_t host_version = {0}, slave_version = {0};
esp_err_t                      ret = ESP_OK;

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
static void printLittleFSContents() {
    DIR* dir = opendir("/littlefs");
    if (dir == nullptr) {
        Serial.println("can't open /littlefs.");
        return;
    }

    Serial.println("content of /littlefs:");

    struct dirent* entry;
    bool           foundFile = false;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { continue; }

        foundFile = true;
        String path = String("/") + entry->d_name;
        File   file = LittleFS.open(path, "r");
        if (!file) {
            Serial.printf("  %s (could not be opened)\n", path.c_str());
            continue;
        }

        Serial.printf("  %s - %u Bytes\n", path.c_str(), static_cast<unsigned>(file.size()));
        file.close();
    }

    if (!foundFile) { Serial.println("  No files found."); }

    closedir(dir);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int compare_versions() {

    host_version.major1 = ESP_HOSTED_VERSION_MAJOR_1;
    host_version.minor1 = ESP_HOSTED_VERSION_MINOR_1;
    host_version.patch1 = ESP_HOSTED_VERSION_PATCH_1;

    ret = esp_hosted_get_coprocessor_fwversion(&slave_version);
    if (ret != ESP_OK) { Serial.printf("ret: %s\n", esp_err_to_name(ret)); }

    uint32_t slave_ver = ESP_HOSTED_VERSION_VAL(slave_version.major1, slave_version.minor1, slave_version.patch1);
    uint32_t host_ver = ESP_HOSTED_VERSION_VAL(host_version.major1, host_version.minor1, host_version.patch1);

    if (host_ver == slave_ver) {
        return 0; // Versions match
    } else if (host_ver > slave_ver) {
        Serial.printf("Version mismatch: Host [%u.%u.%u] > Co-proc [%u.%u.%u] ==> Upgrade co-proc\n", ESP_HOSTED_VERSION_PRINTF_ARGS(host_ver), ESP_HOSTED_VERSION_PRINTF_ARGS(slave_ver));
        return -1; // Host newer, slave needs upgrade
    } else {
        Serial.printf("Version mismatch: Host [%u.%u.%u] < Co-proc [%u.%u.%u] ==> Upgrade host\n", ESP_HOSTED_VERSION_PRINTF_ARGS(host_ver), ESP_HOSTED_VERSION_PRINTF_ARGS(slave_ver));
        return 1; // Slave newer, host needs upgrade
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int perform_slave_ota() {
    uint8_t delete_after_flash = 0;
    Serial.printf("Starting OTA via LittleFS\n");
#ifdef CONFIG_OTA_DELETE_FILE_AFTER_FLASH
    delete_after_flash = 1;
#endif
    return ota_littlefs_perform(delete_after_flash);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void setup() {
    Serial.begin(115200);
    vTaskDelay(1000);
    Serial.print("\n\n");
    Serial.println("----------------------------------");
    Serial.printf("ESP32 Chip: %s\n", ESP.getChipModel());
    Serial.printf("Arduino Version: %d.%d.%d\n", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
    Serial.printf("ESP-IDF Version: %d.%d.%d\n", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
    Serial.printf("ARDUINO_LOOP_STACK_SIZE %d words (32 bit)\n", CONFIG_ARDUINO_LOOP_STACK_SIZE);
    Serial.println("----------------------------------");
    Serial.print("\n\n");

    if (!LittleFS.begin(false, "/littlefs", 10, "storage")) printLittleFSContents();
    LittleFS.end();

    // Step 1: Initialize system
    Serial.print("Initializing ESP-Hosted...\n");
    if (nvs_flash_init() != ESP_OK) Serial.print("nvs_flash_init() failed!\n");
    if (esp_event_loop_create_default() != ESP_OK) Serial.print("esp_event_loop_create_default() failed!\n");
    ;
    if (esp_hosted_init() != ESP_OK) Serial.print("esp_hosted_init() failed!\n");
    if (esp_hosted_connect_to_slave() != ESP_OK) Serial.print("esp_hosted_connect_to_slave() failed!\n");
    Serial.printf("ESP-Hosted initialized successfully\n");

    // Step 2: Check version compatibility
    if (compare_versions() <= 0) {
        Serial.printf("Versions compatible - OTA not required\n");
        return;
    }

    // Step 3: Perform OTA update
    Serial.printf("Starting slave OTA update...\n");
    ret = perform_slave_ota();
    if(ret !=1) Serial.printf("Error %i\n\n", ret);
}

void loop() {
    vTaskDelay(1);
}
