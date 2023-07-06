#include "flipbip_file.h"
#include <storage/storage.h>
#include <loader/loader.h>
#include "../helpers/flipbip_string.h"
#include <furi.h>
#include <furi_hal.h>
#include <flipper_format/flipper_format.h>
// From: lib/crypto
#include <memzero.h>
#include <rand.h>

// #define FLIPBIP_APP_BASE_FOLDER APP_DATA_PATH("flipbip")
#define FLIPBIP_APP_BASE_FOLDER EXT_PATH("apps_data/flipbip")
#define FLIPBIP_APP_BASE_FOLDER_PATH(path) FLIPBIP_APP_BASE_FOLDER "/" path
#define FLIPBIP_DAT_FILE_NAME ".flipbip.dat"
// #define FLIPBIP_DAT_FILE_NAME ".flipbip.dat.txt"
#define FLIPBIP_DAT_FILE_NAME_BAK ".flipbip.dat.bak"
#define FLIPBIP_DAT_PATH FLIPBIP_APP_BASE_FOLDER_PATH(FLIPBIP_DAT_FILE_NAME)
#define FLIPBIP_DAT_PATH_BAK FLIPBIP_APP_BASE_FOLDER_PATH(FLIPBIP_DAT_FILE_NAME_BAK)

const char* TEXT_QRFILE = "Filetype: QRCode\n"
                          "Version: 0\n"
                          "Message: "; // 37 chars + 1 null
#define SETTINGS_MAX_LEN 512
#define FILE_MAX_PATH_LEN 48
#define FILE_MAX_QRFILE_CONTENT 90
#define CRYPTO_KEY_SLOT 11

#define TAG "CR"

bool flipbip_load_file(char* settings, const FlipBipFile file_type, const char* file_name) {
    bool ret = false;
    const char* path;
    if(file_type == FlipBipFileDat) {
        path = FLIPBIP_DAT_PATH;
    } else {
        char path_buf[FILE_MAX_PATH_LEN] = {0};
        strcpy(path_buf, FLIPBIP_APP_BASE_FOLDER); // 22
        strcpy(path_buf + strlen(path_buf), "/");
        strcpy(path_buf + strlen(path_buf), file_name);
        path = path_buf;
    }

    Storage* fs_api = furi_record_open(RECORD_STORAGE);

    File* settings_file = storage_file_alloc(fs_api);
    if(storage_file_open(settings_file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char chr;
        int i = 0;
        while((storage_file_read(settings_file, &chr, 1) == 1) &&
              !storage_file_eof(settings_file) && !isspace(chr)) {
            settings[i] = chr;
            i++;
        }
        ret = true;
    } else {
        memzero(settings, strlen(settings));
        settings[0] = '\0';
        ret = false;
    }
    storage_file_close(settings_file);
    storage_file_free(settings_file);
    furi_record_close(RECORD_STORAGE);

    if(strlen(settings) > 0) {
        Storage* fs_api = furi_record_open(RECORD_STORAGE);
        FileInfo layout_file_info;
        FS_Error file_check_err = storage_common_stat(fs_api, path, &layout_file_info);
        furi_record_close(RECORD_STORAGE);
        if(file_check_err != FSE_OK) {
            memzero(settings, strlen(settings));
            settings[0] = '\0';
            ret = false;
        }
        // if(layout_file_info.size != 256) {
        //     memzero(settings, strlen(settings));
        //     settings[0] = '\0';
        // }
    }

    return ret;
}

bool flipbip_has_file(const FlipBipFile file_type, const char* file_name, const bool remove) {
    bool ret = false;
    const char* path;
    if(file_type == FlipBipFileDat) {
        path = FLIPBIP_DAT_PATH;
    } else {
        char path_buf[FILE_MAX_PATH_LEN] = {0};
        strcpy(path_buf, FLIPBIP_APP_BASE_FOLDER); // 22
        strcpy(path_buf + strlen(path_buf), "/");
        strcpy(path_buf + strlen(path_buf), file_name);
        path = path_buf;
    }

    Storage* fs_api = furi_record_open(RECORD_STORAGE);
    if(remove) {
        ret = storage_simply_remove(fs_api, path);
    } else {
        ret = storage_file_exists(fs_api, path);
    }
    furi_record_close(RECORD_STORAGE);

    return ret;
}

bool flipbip_save_file(
    const char* settings,
    const FlipBipFile file_type,
    const char* file_name,
    const bool append) {
    bool ret = false;
    const char* path;
    const char* path_bak;
    if(file_type == FlipBipFileDat) {
        path = FLIPBIP_DAT_PATH;
        path_bak = FLIPBIP_DAT_PATH_BAK;
    } else {
        char path_buf[FILE_MAX_PATH_LEN] = {0};
        strcpy(path_buf, FLIPBIP_APP_BASE_FOLDER); // 22
        strcpy(path_buf + strlen(path_buf), "/");
        strcpy(path_buf + strlen(path_buf), file_name);
        path = path_buf;
        path_bak = NULL;
    }
    int open_mode = FSOM_OPEN_ALWAYS;
    if(append) {
        open_mode = FSOM_OPEN_APPEND;
    }

    Storage* fs_api = furi_record_open(RECORD_STORAGE);
    // // if the key file exists, we don't want to overwrite it
    // if (key_file && storage_file_exists(fs_api, path)) {
    //     furi_record_close(RECORD_STORAGE);
    //     ret = true;
    //     return ret;
    // }
    // try to create the folder
    storage_simply_mkdir(fs_api, FLIPBIP_APP_BASE_FOLDER);

    File* settings_file = storage_file_alloc(fs_api);
    if(storage_file_open(settings_file, path, FSAM_WRITE, open_mode)) {
        storage_file_write(settings_file, settings, strlen(settings));
        storage_file_write(settings_file, "\n", 1);
        ret = true;
    }
    storage_file_close(settings_file);
    storage_file_free(settings_file);

    if(path_bak != NULL) {
        File* settings_file_bak = storage_file_alloc(fs_api);
        if(storage_file_open(settings_file_bak, path_bak, FSAM_WRITE, open_mode)) {
            storage_file_write(settings_file_bak, settings, strlen(settings));
            storage_file_write(settings_file_bak, "\n", 1);
        }
        storage_file_close(settings_file_bak);
        storage_file_free(settings_file_bak);
    }

    furi_record_close(RECORD_STORAGE);

    return ret;
}

bool flipbip_save_qrfile(
    const char* qr_msg_prefix,
    const char* qr_msg_content,
    const char* file_name) {
    char qr_buf[FILE_MAX_QRFILE_CONTENT] = {0};
    strcpy(qr_buf, TEXT_QRFILE);
    strcpy(qr_buf + strlen(qr_buf), qr_msg_prefix);
    strcpy(qr_buf + strlen(qr_buf), qr_msg_content);
    return flipbip_save_file(qr_buf, FlipBipFileOther, file_name, false);
}

bool flipbip_load_file_secure(char* settings) {
    uint32_t dlen = SETTINGS_MAX_LEN;

    uint8_t* data = malloc(dlen);
    memzero(data, dlen);
    uint8_t iv[16];

    uint32_t version = 0;

    FuriString* filetype;
    filetype = furi_string_alloc();

    // load IVs and encrypted data from file
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    bool success = false;

    if(flipper_format_file_open_existing(flipper_format, FLIPBIP_DAT_PATH)) {
        do {
            if(!flipper_format_read_header(flipper_format, filetype, &version)) {
                FURI_LOG_E(TAG, "Missing or incorrect header");
                break;
            }
            if(strcmp(furi_string_get_cstr(filetype), "FlipBIP encrypted seed file version") !=
                   0 ||
               version != 1) {
                FURI_LOG_E(TAG, "Type or version mismatch");
                break;
            }
            if(!flipper_format_read_hex(flipper_format, "IV", iv, 16)) break;
            if(!flipper_format_read_uint32(flipper_format, "Data length", &dlen, 1)) break;
            if(!flipper_format_read_hex(flipper_format, "Data", data, dlen)) break;
            FURI_LOG_D(TAG, "Data loaded");
            success = true;
        } while(0);
    }

    // close file
    flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);
    free(filetype);

    if(!success) {
        FURI_LOG_E(TAG, "Unable to load data");
        memzero(data, dlen);
        free(data);
        return false;
    }

    // prepare crypto
    if(!furi_hal_crypto_store_load_key(CRYPTO_KEY_SLOT, iv)) {
        FURI_LOG_E(TAG, "Unable to load encryption key");
        memzero(data, dlen);
        free(data);
        return false;
    }

    // prepare memory for decrypted data
    uint8_t* settings_bin = malloc(dlen);
    memzero(settings_bin, dlen);

    // decrypt data to output
    if(!furi_hal_crypto_decrypt(data, settings_bin, dlen)) {
        FURI_LOG_E(TAG, "Decryption failed");
        memzero(data, dlen);
        free(data);
        memzero(settings_bin, dlen);
        free(settings_bin);
        furi_hal_crypto_store_unload_key(CRYPTO_KEY_SLOT);
        return false;
    } else {
        FURI_LOG_D(TAG, "Decryption successful");
    }

    // deinit crypto
    furi_hal_crypto_store_unload_key(CRYPTO_KEY_SLOT);

    // copy decrypted data to output
    strcpy(settings, (char*)settings_bin);

    // clear memory
    memzero(data, dlen);
    free(data);
    memzero(settings_bin, dlen);
    free(settings_bin);

    return true;
}

bool flipbip_save_file_secure(const char* settings) {
    // cap settings to 256 chars
    uint32_t len = strlen(settings);
    if(len > (SETTINGS_MAX_LEN / 2)) {
        len = SETTINGS_MAX_LEN / 2;
        FURI_LOG_D(TAG, "Settings too long, truncating to %ld chars", len);
    }

    // pad settings
    uint8_t* padded_settings = malloc(SETTINGS_MAX_LEN);
    memzero(padded_settings, SETTINGS_MAX_LEN);
    strcpy((char*)padded_settings, settings);
    len = SETTINGS_MAX_LEN;

    // allocate memory for data
    uint8_t* data = malloc(len);
    memzero(data, len);

    // check if the key exists, if not, create it
    if(!furi_hal_crypto_verify_key(CRYPTO_KEY_SLOT)) return false;

    // prepare IVs
    uint8_t iv[16];
    furi_hal_random_fill_buf(iv, 16);
    if(!furi_hal_crypto_store_load_key(CRYPTO_KEY_SLOT, iv)) {
        FURI_LOG_E(TAG, "Unable to load encryption key");
        memzero(data, len);
        free(data);
        memzero(padded_settings, len);
        free(padded_settings);
        memzero(iv, 16);
        return false;
    } else {
        FURI_LOG_D(TAG, "Encryption key loaded");
    }

    if(!furi_hal_crypto_encrypt(padded_settings, data, len)) {
        FURI_LOG_E(TAG, "Encryption failed");
        memzero(data, len);
        free(data);
        memzero(padded_settings, len);
        free(padded_settings);
        memzero(iv, 16);
        return false;
    } else {
        FURI_LOG_D(TAG, "Encryption successful");
    }

    // deinit crypto
    furi_hal_crypto_store_unload_key(CRYPTO_KEY_SLOT);

    // free padded settings
    memzero(padded_settings, len);
    free(padded_settings);

    // save data to file
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);
    bool success = false;

    if(flipper_format_file_open_always(flipper_format, FLIPBIP_DAT_PATH)) {
        do {
            if(!flipper_format_write_header_cstr(
                   flipper_format, "FlipBIP encrypted seed file version", 1))
                break;
            if(!flipper_format_write_hex(flipper_format, "IV", iv, 16)) break;
            if(!flipper_format_write_uint32(flipper_format, "Data length", &len, 1)) break;
            if(!flipper_format_write_hex(flipper_format, "Data", data, len)) break;
            success = true;
        } while(0);
    }

    FlipperFormat* flipper_format_bak = flipper_format_file_alloc(storage);

    // save the backup file
    if(flipper_format_file_open_always(flipper_format_bak, FLIPBIP_DAT_PATH_BAK)) {
        do {
            if(!flipper_format_write_header_cstr(
                   flipper_format_bak, "FlipBIP encrypted seed file version", 1))
                break;
            if(!flipper_format_write_hex(flipper_format_bak, "IV", iv, 16)) break;
            if(!flipper_format_write_uint32(flipper_format_bak, "Data length", &len, 1)) break;
            if(!flipper_format_write_hex(flipper_format_bak, "Data", data, len)) break;
            success = true;
        } while(0);
    }

    // close file
    flipper_format_free(flipper_format);
    flipper_format_free(flipper_format_bak);
    furi_record_close(RECORD_STORAGE);

    // clear memory
    memzero(data, len);
    free(data);
    memzero(iv, 16);

    return success;
}