/*********************
 *      INCLUDES
 *********************/
#include "esp_log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "app_oem.h"
#include "string.h"
#include "littlefs_utils.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "app_oem"
#define OEM_CONFIG_OEM_IMAGE_FILE "oem_image.bin"

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
bool app_oem_read(char *oem_partition_label, oem_image_t **oem_image);
bool app_oem_write(char *oem_partition_label, oem_image_t *oem_image);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
bool app_oem_read(char *oem_partition_label, oem_image_t **oem_image)
{
    uint8_t *buffer = NULL;
    size_t size = 0;
    bool ret = littlefs_read_file(oem_partition_label, OEM_CONFIG_OEM_IMAGE_FILE, &buffer, &size);
    if (!ret)
    {
        ESP_LOGE(TAG, "failed to read oem image file");
        return false;
    }
    *oem_image = (oem_image_t *)buffer;
    return true;
}
bool app_oem_write(char *oem_partition_label, oem_image_t *oem_image)
{
    bool ret = littlefs_write_file(oem_partition_label, OEM_CONFIG_OEM_IMAGE_FILE, (uint8_t *)oem_image, sizeof(oem_image_t));
    if (!ret)
    {
        ESP_LOGE(TAG, "failed to write oem image file");
        return false;
    }
    return true;
}