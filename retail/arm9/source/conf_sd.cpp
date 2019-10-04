#include <stdlib.h> // strtol
#include <unistd.h>
//#include <stdio.h>
#include <nds.h>
#include <string>
#include <string.h>
#include <limits.h> // PATH_MAX
/*#include <nds/ndstypes.h>
#include <nds/fifocommon.h>
#include <nds/arm9/console.h>
#include <nds/debug.h>*/
#include <fat.h>
#include "lzss.h"
#include "minIni.h"
#include "hex.h"
#include "cheat_engine.h"
#include "configuration.h"
#include "conf_sd.h"
#include "nitrofs.h"
#include "locations.h"

extern std::string patchOffsetCacheFilePath;

extern u8 lz77ImageBuffer[0x10000];

static off_t getFileSize(const char* path) {
	FILE* fp = fopen(path, "rb");
	off_t fsize = 0;
	if (fp) {
		fseek(fp, 0, SEEK_END);
		fsize = ftell(fp);
		if (!fsize) fsize = 0;
		fclose(fp);
	}
	return fsize;
}

extern std::string ReplaceAll(std::string str, const std::string& from, const std::string& to);

static inline bool match(const char* section, const char* s, const char* key, const char* k) {
	return (strcmp(section, s) == 0 && strcmp(key, k) == 0);
}

static int callback(const char *section, const char *key, const char *value, void *userdata) {
	configuration* conf = (configuration*)userdata;

	if (match(section, "NDS-BOOTSTRAP", key, "DEBUG")) {
		// Debug
		conf->debug = (bool)strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "NDS_PATH")) {
		// NDS path
		//conf->ndsPath = malloc((strlen(value) + 1)*sizeof(char));
		//strcpy(conf->ndsPath, value);
		conf->ndsPath = strdup(value);

	} else if (match(section, "NDS-BOOTSTRAP", key, "SAV_PATH")) {
		// SAV path
		//conf->savPath = malloc((strlen(value) + 1)*sizeof(char));
		//strcpy(conf->savPath, value);
		conf->savPath = strdup(value);

	} else if (match(section, "NDS-BOOTSTRAP", key, "AP_FIX_PATH")) {
		// AP fix path
		//conf->apPatchPath = malloc((strlen(value) + 1)*sizeof(char));
		//strcpy(conf->apPatchPath, value);
		conf->apPatchPath = strdup(value);

	} else if (match(section, "NDS-BOOTSTRAP", key, "LANGUAGE")) {
		// Language
		conf->language = strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "DSI_MODE")) {
		// DSi mode
		conf->dsiMode = (bool)strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "DONOR_SDK_VER")) {
		// Donor SDK version
		conf->donorSdkVer = strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "PATCH_MPU_REGION")) {
		// Patch MPU region
		conf->patchMpuRegion = strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "PATCH_MPU_SIZE")) {
		// Patch MPU size
		conf->patchMpuSize = strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "CONSOLE_MODEL")) {
		// Console model
		conf->consoleModel = strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "ROMREAD_LED")) {
		// ROM read LED
		conf->romread_LED = strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "GAME_SOFT_RESET")) {
		// Game soft reset
		conf->gameSoftReset = (bool)strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "FORCE_SLEEP_PATCH")) {
		// Force sleep patch
		conf->forceSleepPatch = (bool)strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "LOGGING")) {
		// Logging
		conf->logging = (bool)strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "BACKLIGHT_MODE")) {
		// Backlight mode
		conf->backlightMode = strtol(value, NULL, 0);

	} else if (match(section, "NDS-BOOTSTRAP", key, "BOOST_CPU")) {
		// Boost CPU
		if (conf->dsiMode) {
			conf->boostCpu = true;
		} else {
			conf->boostCpu = (bool)strtol(value, NULL, 0);
		}

	} else if (match(section, "NDS-BOOTSTRAP", key, "BOOST_VRAM")) {
		// Boost VRAM
		if (conf->dsiMode) {
			conf->boostVram = true;
		} else {
			conf->boostVram = (bool)strtol(value, NULL, 0);
		}

	} else {
		// Unknown section/name
		//return 0; // Error
	}
	
	return 1; // Continue searching
}

int loadFromSD(configuration* conf, const char *bootstrapPath) {
	if (!fatInitDefault()) {
		consoleDemoInit();
		printf("FAT init failed!\n");
		return -1;
	}
	nocashMessage("fatInitDefault");

	if ((access("fat:/", F_OK) != 0) && (access("sd:/", F_OK) == 0)) {
		consoleDemoInit();
		printf("This edition of nds-bootstrap:\n");
		printf("B4DS, can only be used\n");
		printf("on a flashcard.\n");
		return -1;
	}
	
	ini_browse(callback, conf, "fat:/_nds/nds-bootstrap.ini");
	mkdir("fat:/_nds", 0777);
	mkdir("fat:/_nds/nds-bootstrap", 0777);
	mkdir("fat:/_nds/nds-bootstrap/B4DS-patchOffsetCache", 0777);

	if (!nitroFSInit(bootstrapPath)) {
		consoleDemoInit();
		printf("nitroFSInit failed!\n");
		return -1;
	}
	
	// Load ce7 binary
	FILE* cebin = fopen("nitro:/cardengine_arm7.bin", "rb");
	if (cebin) {
		fread((void*)CARDENGINE_ARM7_LOCATION, 1, 0x1000, cebin);
	}
	fclose(cebin);
    
	conf->romSize = getFileSize(conf->ndsPath);
	conf->saveSize = getFileSize(conf->savPath);
	conf->apPatchSize = getFileSize(conf->apPatchPath);

	FILE* bootstrapImages = fopen("nitro:/bootloader_images.lz77", "rb");
	if (bootstrapImages) {
		fread(lz77ImageBuffer, 1, 0x8000, bootstrapImages);
		LZ77_Decompress(lz77ImageBuffer, (u8*)0x02350000);
	}
	fclose(bootstrapImages);

	std::string romFilename = ReplaceAll(conf->ndsPath, ".nds", ".bin");
	const size_t last_slash_idx = romFilename.find_last_of("/");
	if (std::string::npos != last_slash_idx)
	{
		romFilename.erase(0, last_slash_idx + 1);
	}

	patchOffsetCacheFilePath = "fat:/_nds/nds-bootstrap/B4DS-patchOffsetCache/"+romFilename;
	
	if (access(patchOffsetCacheFilePath.c_str(), F_OK) != 0) {
		char buffer[0x200] = {0};

		FILE* patchOffsetCacheFile = fopen(patchOffsetCacheFilePath.c_str(), "wb");
		fwrite(buffer, 1, sizeof(buffer), patchOffsetCacheFile);
		fclose(patchOffsetCacheFile);
	}

	return 0;
}