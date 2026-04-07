/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ESP-Hosted LittleFS OTA Component
 * ================================
 *
 * Reads ESP32 slave firmware from LittleFS filesystem and performs OTA update.
 *
 * FEATURES:
 * - Firmware validation (magic number, image header)
 * - Version checking against current slave firmware
 * - Chunked transfer to slave device
 * - Optional firmware file deletion after successful flash
 *
 * APIs USED:
 * - esp_hosted_get_coprocessor_fwversion() - Get slave firmware version
 * - esp_hosted_slave_ota_begin()  - Initialize OTA session
 * - esp_hosted_slave_ota_write()  - Transfer firmware chunks
 * - esp_hosted_slave_ota_end()    - Finalize OTA session
 * - esp_hosted_slave_ota_activate() - (if current slave FW > v2.5.X only) - ** Called from main.c **
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_hosted_ota.h"
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_format.h"
#include "esp_app_desc.h"
#include "esp_hosted.h"
#include "esp_hosted_api_types.h"


#ifndef CHUNK_SIZE
#define CHUNK_SIZE 1500
#endif

/* Function to parse ESP32 image header and get firmware info from file */
static esp_err_t parse_image_header_from_file(const char* file_path, size_t* firmware_size, char* app_version_str, size_t version_str_len)
{
	FILE* file;
	esp_image_header_t image_header;
	esp_image_segment_header_t segment_header;
	esp_app_desc_t app_desc;
	size_t offset = 0;
	size_t total_size = 0;

	file = fopen(file_path, "rb");
	if (file == NULL) {
		printf("Error: Failed to open firmware file for header verification: %s\n", file_path);
		return ESP_FAIL;
	}

	/* Read image header */
	if (fread(&image_header, sizeof(image_header), 1, file) != 1) {
		printf("Error: Failed to read image header from file");
		fclose(file);
		return ESP_FAIL;
	}

	/* Validate magic number */
	if (image_header.magic != ESP_IMAGE_HEADER_MAGIC) {
		printf("Invalid image magic: 0x%" PRIx8 " (expected: 0x%" PRIx8 ")\n", image_header.magic, ESP_IMAGE_HEADER_MAGIC);
		printf("This indicates the file is not a valid ESP32 firmware image!\n");
		printf("Please ensure you have flashed the correct firmware binary to the LittleFS partition.\n");
		fclose(file);
		return ESP_ERR_INVALID_ARG;
	}

	printf("Image header: magic=0x%" PRIx8 ", segment_count=%" PRIu8 ", hash_appended=%" PRIu8 "\n",
			image_header.magic, image_header.segment_count, image_header.hash_appended);

	/* Calculate total size by reading all segments */
	offset = sizeof(image_header);
	total_size = sizeof(image_header);

	for (int i = 0; i < image_header.segment_count; i++) {
		/* Read segment header */
		if (fseek(file, offset, SEEK_SET) != 0 ||
				fread(&segment_header, sizeof(segment_header), 1, file) != 1) {
			printf("Error: Failed to read segment %d header\n", i);
			fclose(file);
			return ESP_FAIL;
		}

		printf("Segment %d: data_len=%" PRIu32 ", load_addr=0x%" PRIx32 "\n", i, segment_header.data_len, segment_header.load_addr);

		/* Add segment header size + data size */
		total_size += sizeof(segment_header) + segment_header.data_len;
		offset += sizeof(segment_header) + segment_header.data_len;

		/* Read app description from the first segment */
		if (i == 0) {
			size_t app_desc_offset = sizeof(image_header) + sizeof(segment_header);
			if (fseek(file, app_desc_offset, SEEK_SET) == 0 &&
					fread(&app_desc, sizeof(app_desc), 1, file) == 1) {
				strncpy(app_version_str, app_desc.version, version_str_len - 1);
				app_version_str[version_str_len - 1] = '\0';
				printf("Found app description: version='%s', project_name='%s'\n",
						app_desc.version, app_desc.project_name);
			} else {
				printf("Warn: Failed to read app description\n");
				strncpy(app_version_str, "unknown", version_str_len - 1);
				app_version_str[version_str_len - 1] = '\0';
			}
		}
	}

	/* Add padding to align to 16 bytes */
	size_t padding = (16 - (total_size % 16)) % 16;
	if (padding > 0) {
		printf("Debug: Adding %u bytes of padding for alignment\n", (unsigned int)padding);
		total_size += padding;
	}

	/* Add the checksum byte (always present) */
	total_size += 1;
	printf("Debug: Added 1 byte for checksum\n");

	/* Add SHA256 hash if appended */
	bool has_hash = (image_header.hash_appended == 1);
	if (has_hash) {
		total_size += 32;  // SHA256 hash is 32 bytes
		printf("Debug: Added 32 bytes for SHA256 hash (hash_appended=1)\n");
	} else {
		printf("Debug: No SHA256 hash appended (hash_appended=0)\n");
	}

	*firmware_size = total_size;
	printf("Total image size: %u bytes\n", (unsigned int)*firmware_size);

	fclose(file);
	return ESP_OK;
}

/* Find latest firmware file in LittleFS */
static esp_err_t find_latest_firmware(char* firmware_path, size_t max_len)
{
	DIR *dir;
	struct dirent *entry;
	struct stat file_stat;
	char *latest_file = malloc(256); // Use heap instead of stack
	char *full_path = malloc(512);   // Use heap for full path

	if (!latest_file || !full_path) {
		printf("Error: Failed to allocate memory for file search\n");
		if (latest_file) free(latest_file);
		if (full_path) free(full_path);
		return ESP_ERR_NO_MEM;
	}

	memset(latest_file, 0, 256);

	dir = opendir("/littlefs");
	if (dir == NULL) {
		printf("Error: Failed to open /littlefs directory\n");
		free(latest_file);
		free(full_path);
		return ESP_FAIL;
	}

	printf("Successfully opened /littlefs directory\n");

	/* Find the first .bin file (since timestamps might not be reliable in LittleFS) */
	while ((entry = readdir(dir)) != NULL) {
		printf("Found file: %s\n", entry->d_name);
		if (strstr(entry->d_name, ".bin") != NULL) {
			printf("Found .bin file: %s\n", entry->d_name);
			snprintf(full_path, 512, "/littlefs/%s", entry->d_name);

			if (stat(full_path, &file_stat) == 0) {
				printf("File stat successful for %s, size: %ld\n", entry->d_name, file_stat.st_size);
				/* Use the first .bin file found */
				strncpy(latest_file, entry->d_name, 255);
				latest_file[255] = '\0'; // Ensure null termination
				printf("Using firmware file: %s\n", latest_file);
				break; // Use the first .bin file found
			} else {
				printf("Warn: Failed to stat file: %s\n", full_path);
			}
		}
	}
	closedir(dir);

	printf("Final latest_file: '%s', length: %d\n", latest_file, strlen(latest_file));

	if (strlen(latest_file) == 0) {
		printf("Error: No valid .bin firmware files found in /littlefs directory!\n");
		printf("Error: Please ensure:\n");
		printf("Error:   - The firmware binary has a .bin extension\n");
		printf("Error:   - The binary is flashed to the 'storage' partition\n");
		printf("Error:   - The binary is a valid ESP32 firmware image\n");
		printf("Error: Refer to documentation for partition table setup and flashing instructions.\n");
		free(latest_file);
		free(full_path);
		return ESP_FAIL;
	}

	// Ensure we don't overflow the destination buffer
	if (snprintf(firmware_path, max_len, "/littlefs/%s", latest_file) >= max_len) {
		printf("Error: Firmware path too long\n");
		free(latest_file);
		free(full_path);
		return ESP_FAIL;
	}

	printf("Found latest firmware: %s\n", firmware_path);

	// Clean up allocated memory
	free(latest_file);
	free(full_path);

	return ESP_OK;
}

/* Function to check if LittleFS partition has any files */
static esp_err_t check_littlefs_files(void)
{
	DIR *dir;
	struct dirent *entry;
	int file_count = 0;

	dir = opendir("/littlefs");
	if (dir == NULL) {
		printf("Error: Failed to open /littlefs directory\n");
		return ESP_FAIL;
	}

	printf("Checking contents of /littlefs partition:\n");

	while ((entry = readdir(dir)) != NULL) {
		/* Skip . and .. directories */
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		file_count++;
		printf("  Found: %s\n", entry->d_name);
	}
	closedir(dir);

	if (file_count == 0) {
		printf("Warn: LittleFS partition is empty! No firmware files found.\n");
		printf("Warn: Please ensure you have:\n");
		printf("Warn:   1. Created a 'storage' partition in your partition table\n");
		printf("Warn:   2. Flashed a firmware binary to the LittleFS partition\n");
		printf("Warn:   3. Or used 'idf.py flash' with the firmware binary\n");
		printf("Warn: Refer to the documentation for detailed setup instructions.\n");
		return ESP_ERR_NOT_FOUND;
	}

	printf("Found %d file(s) in LittleFS partition\n", file_count);
	return ESP_OK;
}

esp_err_t ota_littlefs_perform(bool delete_after_use)
{
	char *firmware_path = malloc(256); // Use heap instead of stack
	FILE *firmware_file;
	uint8_t *chunk = malloc(CHUNK_SIZE); // Use heap for chunk buffer
	size_t bytes_read;
	esp_err_t ret = ESP_OK;

	if (!firmware_path || !chunk) {
		printf("Error: Failed to allocate memory\n");
		if (firmware_path) free(firmware_path);
		if (chunk) free(chunk);
		return ESP_ERR_NO_MEM;
	}

	printf("Starting LittleFS OTA process\n");

	/* Initialize LittleFS */
	printf("Initializing LittleFS filesystem\n");
	esp_vfs_littlefs_conf_t conf = {
		.base_path = "/littlefs",
		.partition_label = "storage",
		.format_if_mount_failed = true,
		.dont_mount = false,
	};

	ret = esp_vfs_littlefs_register(&conf);
	if (ret != ESP_OK) {
		printf("Error: Failed to initialize LittleFS: %s\n", esp_err_to_name(ret));
		free(firmware_path);
		free(chunk);
		return ESP_HOSTED_SLAVE_OTA_FAILED;
	}

	printf("LittleFS filesystem registered successfully\n");

	/* Check if LittleFS partition has any files */
	ret = check_littlefs_files();
	if (ret == ESP_ERR_NOT_FOUND) {
		printf("Warn: OTA cannot proceed - no firmware files found in LittleFS partition\n");
		esp_vfs_littlefs_unregister("storage");
		free(firmware_path);
		free(chunk);
		return ESP_HOSTED_SLAVE_OTA_FAILED;
	} else if (ret != ESP_OK) {
		printf("Error: Failed to check LittleFS partition contents\n");
		esp_vfs_littlefs_unregister("storage");
		free(firmware_path);
		free(chunk);
		return ESP_HOSTED_SLAVE_OTA_FAILED;
	}

	/* Get filesystem info */
	size_t total = 0, used = 0;
	ret = esp_littlefs_info("storage", &total, &used);
	if (ret != ESP_OK) {
		printf("Warn: Failed to get LittleFS partition information (%s)\n", esp_err_to_name(ret));
	} else {
		printf("LittleFS partition size: total: %d, used: %d\n", total, used);
	}

	/* Find the latest firmware file */
	printf("Searching for firmware files in LittleFS\n");
	ret = find_latest_firmware(firmware_path, 256);
	if (ret != ESP_OK) {
		printf("Error: Failed to find firmware file\n");
		esp_vfs_littlefs_unregister("storage");
		free(firmware_path);
		free(chunk);
		return ESP_HOSTED_SLAVE_OTA_FAILED;
	}
	printf("Firmware file found: %s\n", firmware_path);

	/* Verify image header and get firmware info */
	size_t firmware_size;
	char new_app_version[32];
	ret = parse_image_header_from_file(firmware_path, &firmware_size, new_app_version, sizeof(new_app_version));
	if (ret != ESP_OK) {
		printf("Error: Failed to parse image header: %s\n", esp_err_to_name(ret));
		esp_vfs_littlefs_unregister("storage");
		free(firmware_path);
		free(chunk);
		return ESP_HOSTED_SLAVE_OTA_FAILED;
	}

	printf("Firmware verified - Size: %u bytes, Version: %s\n", (unsigned int)firmware_size, new_app_version);

#ifdef CONFIG_OTA_VERSION_CHECK_SLAVEFW_SLAVE
	/* Get current running slave firmware version */
	esp_hosted_coprocessor_fwver_t current_slave_version = {0};
	esp_err_t version_ret = esp_hosted_get_coprocessor_fwversion(&current_slave_version);

	if (version_ret == ESP_OK) {
		char current_version_str[32];
		snprintf(current_version_str, sizeof(current_version_str), "%" PRIu32 ".%" PRIu32 ".%" PRIu32,
				current_slave_version.major1, current_slave_version.minor1, current_slave_version.patch1);

		printf("Current slave firmware version: %s\n", current_version_str);
		printf("New slave firmware version: %s\n", new_app_version);

		if (strcmp(new_app_version, current_version_str) == 0) {
			printf("Warn: Current slave firmware version (%s) is the same as new version (%s). Skipping OTA.\n",
					current_version_str, new_app_version);
			esp_vfs_littlefs_unregister("storage");
			free(firmware_path);
			free(chunk);
			return ESP_HOSTED_SLAVE_OTA_NOT_REQUIRED;
		}

		printf("Version differs - proceeding with OTA from %s to %s\n", current_version_str, new_app_version);
	} else {
		printf("Warn: Could not get current slave firmware version (error: %s), proceeding with OTA\n",
				esp_err_to_name(version_ret));
	}
#else
	printf("Version check disabled - proceeding with OTA (new firmware version: %s)\n", new_app_version);
#endif

	/* Open firmware file */
	firmware_file = fopen(firmware_path, "rb");
	if (firmware_file == NULL) {
		printf("Error: Failed to open firmware file: %s\n", firmware_path);
		esp_vfs_littlefs_unregister("storage");
		free(firmware_path);
		free(chunk);
		return ESP_FAIL;
	}

	printf("Starting OTA from LittleFS: %s\n", firmware_path);

	/* Begin OTA */
	ret = esp_hosted_slave_ota_begin();
	if (ret != ESP_OK) {
		printf("Error: Failed to begin OTA: %s\n", esp_err_to_name(ret));
		fclose(firmware_file);
		esp_vfs_littlefs_unregister("storage");
		free(firmware_path);
		free(chunk);
		return ESP_HOSTED_SLAVE_OTA_FAILED;
	}

	/* Write firmware in chunks */
	while ((bytes_read = fread(chunk, 1, CHUNK_SIZE, firmware_file)) > 0) {
		ret = esp_hosted_slave_ota_write(chunk, bytes_read);
		if (ret != ESP_OK) {
			printf("Error: Failed to write OTA chunk: %s\n", esp_err_to_name(ret));
			fclose(firmware_file);
			esp_vfs_littlefs_unregister("storage");
			free(firmware_path);
			free(chunk);
			return ESP_HOSTED_SLAVE_OTA_FAILED;
		}
	}

	fclose(firmware_file);

	/* End OTA */
	ret = esp_hosted_slave_ota_end();
	if (ret != ESP_OK) {
		printf("Error: Failed to end OTA: %s\n", esp_err_to_name(ret));
		esp_vfs_littlefs_unregister("storage");
		free(firmware_path);
		free(chunk);
		return ESP_HOSTED_SLAVE_OTA_FAILED;
	}

	printf("LittleFS OTA completed successfully\n");

	/* Delete firmware file if requested */
	if (delete_after_use) {
		if (unlink(firmware_path) == 0) {
			printf("Deleted firmware file: %s\n", firmware_path);
		} else {
			printf("Warn: Failed to delete firmware file: %s\n", firmware_path);
		}
	}

	esp_vfs_littlefs_unregister("storage");

	/* Clean up allocated memory */
	free(firmware_path);
	free(chunk);

	return ESP_HOSTED_SLAVE_OTA_COMPLETED;
}
