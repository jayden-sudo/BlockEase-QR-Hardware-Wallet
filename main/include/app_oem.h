#ifndef APP_OEM_H
#define APP_OEM_H

/*********************
 *      INCLUDES
 *********************/
#include "esp_err.h"
#include <stdbool.h>
#include "esp_lvgl_port.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct __attribute__((aligned(4)))
{
    lv_image_header_t header; /**< A header describing the basics of the image*/
    uint32_t data_size;       /**< Size of the image in bytes*/
    uint8_t data[1024 * 512]; /**< the data of the image, max 512kb*/
} oem_image_t;

#ifdef __cplusplus
extern "C"
{
#endif

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    bool app_oem_read(char *oem_partition_label, oem_image_t **oem_image);
    bool app_oem_write(char *oem_partition_label, oem_image_t *oem_image);

#ifdef __cplusplus
}
#endif

#endif // APP_PERIPHERALS_H