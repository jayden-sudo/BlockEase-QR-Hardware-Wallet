/*********************
 *      INCLUDES
 *********************/
#include "littlefs_utils.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

/*********************
 *      DEFINES
 *********************/
#define TAG "littlefs"
#define BASE_PATH "/oem"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *      VARIABLES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static esp_vfs_littlefs_conf_t conf = {
    .base_path = BASE_PATH,
    .partition_label = "--",
    .format_if_mount_failed = true,
    .dont_mount = false,
};

/**********************
 *  STATIC PROTOTYPES
 **********************/
static bool mount_littlefs(const char *partition_label);
static bool unmount_littlefs(const char *partition_label);

/**********************
 * GLOBAL PROTOTYPES
 **********************/
bool littlefs_format(const char *partition_label);
bool littlefs_read_file(const char *partition_label, const char *filename, uint8_t **buffer, size_t *size);
bool littlefs_write_file(const char *partition_label, const char *filename, const uint8_t *buffer, size_t size);

/**********************
 *   STATIC FUNCTIONS
 **********************/
static bool mount_littlefs(const char *partition_label)
{
    conf.partition_label = partition_label;
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return false;
    }
    return true;
}
static bool unmount_littlefs(const char *partition_label)
{
    conf.partition_label = partition_label;
    esp_err_t ret = esp_vfs_littlefs_unregister(conf.partition_label);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unmount LittleFS (%s)", esp_err_to_name(ret));
    }
    return true;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
bool littlefs_format(const char *partition_label)
{
    conf.partition_label = partition_label;
    esp_err_t ret = esp_littlefs_format(conf.partition_label);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to format LittleFS (%s)", esp_err_to_name(ret));
        return false;
    }
    return true;
}
bool littlefs_read_file(const char *partition_label, const char *filename, uint8_t **buffer, size_t *size)
{
    if (!mount_littlefs(partition_label))
    {
        return false;
    }
    char *path = malloc(strlen(filename) + 10);

    sprintf(path, "%s/%s", BASE_PATH, filename);

    struct stat st;
    if (stat(path, &st) == 0)
    {
        *buffer = malloc(st.st_size);
        if (*buffer == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate memory");
            goto done;
        }
        FILE *f = fopen(path, "rb");
        if (f == NULL)
        {
            ESP_LOGE(TAG, "Failed to open file for reading");
            free(*buffer);
            *buffer = NULL;
            goto done;
        }
        fread(*buffer, sizeof(char), st.st_size, f);
        fclose(f);
        *size = st.st_size;
        goto done;
    }
    else
    {
        *size = 0;
        *buffer = NULL;
        goto done;
    }

done:
    free(path);
    unmount_littlefs(partition_label);
    return true;
}
bool littlefs_write_file(const char *partition_label, const char *filename, const uint8_t *buffer, size_t size)
{
    if (!mount_littlefs(partition_label))
    {
        ESP_LOGE("littlefs", "failed to mount littlefs");
        return false;
    }

    char *path = malloc(strlen(filename) + 10);

    sprintf(path, "%s/%s", BASE_PATH, filename);
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to create file");
        free(path);
        unmount_littlefs(partition_label);
        return false;
    }
    fwrite(buffer, 1, size, f);
    fclose(f);
    free(path);
    unmount_littlefs(partition_label);
    return true;
}
