#ifndef AES_STR_H
#define AES_STR_H

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <stddef.h>

/*********************
 *      DEFINES
 *********************/
#define AES_BLOCK_SIZE 16

#ifdef __cplusplus
extern "C"
{
#endif

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    int aes_encrypt(const uint8_t key[32], const uint8_t *plaintext,
                    size_t len, uint8_t *ciphertext);
    int aes_decrypt(const uint8_t key[32], const uint8_t *ciphertext,
                    size_t len, uint8_t *plaintext);

#ifdef __cplusplus
}
#endif

#endif /* AES_STR_H */
