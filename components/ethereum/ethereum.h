#ifndef ETHEREUM_H
#define ETHEREUM_H

/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif
    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    bool ethereum_typed_data_hash_v4(
        const char *json_str_primary_type,
        const char *json_str_types,
        const char *json_str_domain,
        const char *json_str_message,
        uint8_t typed_data_hash[32]);

    void ethereum_keccak256(const unsigned char *data, size_t len, uint8_t digest[32]);
    void ethereum_keccak256_eip191(const char *data, size_t len, uint8_t digest[32]);

#ifdef __cplusplus
}
#endif

#endif // ETHEREUM_H
