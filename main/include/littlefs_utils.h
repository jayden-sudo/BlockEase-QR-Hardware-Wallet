#ifndef LITTLEFS_UTILS_H
#define LITTLEFS_UTILS_H

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

#ifdef __cplusplus
extern "C"
{
#endif

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    bool littlefs_format(const char *partition_label);
    bool littlefs_read_file(const char *partition_label, const char *filename, uint8_t **buffer, size_t *size);
    bool littlefs_write_file(const char *partition_label, const char *filename, const uint8_t *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif // LITTLEFS_UTILS_H