/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include "ethereum.h"
#include "esp_log.h"
#include "eip712/eip712.h"
#include "utility/trezor/sha3.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "ETHEREUM"
// #define ETHEREUM_SIG_PREFIX "\x19Ethereum Signed Message:\n"
#define ETHEREUM_SIG_PREFIX "Ethereum Signed Message:\n"

/**********************
 *      VARIABLES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
bool ethereum_typed_data_hash_v4(
    const char *json_str_primary_type,
    const char *json_str_types,
    const char *json_str_domain,
    const char *json_str_message,
    uint8_t typed_data_hash[32]);

void ethereum_keccak256(const uint8_t *data, size_t len, uint8_t digest[32]);
void ethereum_keccak256_eip191(const char *data, size_t len, uint8_t digest[32]);

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
bool ethereum_typed_data_hash_v4(
    const char *json_str_primary_type,
    const char *json_str_types,
    const char *json_str_domain,
    const char *json_str_message,
    uint8_t typed_data_hash[32])
{
    memset(typed_data_hash, 0, 32);
    if (json_str_domain == NULL || json_str_message == NULL || json_str_primary_type == NULL || json_str_types == NULL)
    {
        return false;
    }
    return eip712_typed_data_hash_v4(json_str_primary_type, json_str_types, json_str_domain, json_str_message, typed_data_hash);
}

void ethereum_keccak256(const uint8_t *data, size_t len, uint8_t digest[32])
{
    keccak_256(data, len, digest);
}

void ethereum_keccak256_eip191(const char *data, size_t len, uint8_t digest[32])
{
    char *data_len_str = (char *)malloc(20);
    sprintf(data_len_str, "%zu", len);

    char *_ETHEREUM_SIG_PREFIX = malloc(strlen(ETHEREUM_SIG_PREFIX) + 2);
    _ETHEREUM_SIG_PREFIX[0] = 0x19;
    strcpy(_ETHEREUM_SIG_PREFIX + 1, ETHEREUM_SIG_PREFIX);
    size_t _ETHEREUM_SIG_PREFIX_len = strlen(_ETHEREUM_SIG_PREFIX);

    size_t joined_data_len = _ETHEREUM_SIG_PREFIX_len + strlen(data_len_str) + len;
    char *joined_data = malloc(joined_data_len);
    sprintf(joined_data, "%s%s", _ETHEREUM_SIG_PREFIX, data_len_str);
    memcpy(joined_data + _ETHEREUM_SIG_PREFIX_len + strlen(data_len_str), data, len);

    free(_ETHEREUM_SIG_PREFIX);
    free(data_len_str);
    ethereum_keccak256((uint8_t *)joined_data, joined_data_len, digest);
    free(joined_data);
}
