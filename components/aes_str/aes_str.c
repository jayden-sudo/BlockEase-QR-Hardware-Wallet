/*********************
 *      INCLUDES
 *********************/
#include "aes_str.h"
#include <mbedtls/aes.h>
#include <errno.h>
#include <esp_log.h>

#ifndef MBEDTLS_AES_C
#error "not implemented"
#endif

/*********************
 *      DEFINES
 *********************/
static const char *TAG = "aes_str";

/**********************
 *  STATIC PROTOTYPES
 **********************/
static inline int aes_set_key(mbedtls_aes_context *ctx, const uint8_t *key);
static int aes_crypt_block(mbedtls_aes_context *ctx, int mode,
						   const uint8_t *input, uint8_t *output);

/**********************
 * GLOBAL PROTOTYPES
 **********************/
int aes_encrypt(const uint8_t key[32], const uint8_t *plaintext,
				size_t len, uint8_t *ciphertext);
int aes_decrypt(const uint8_t key[32], const uint8_t *ciphertext,
				size_t len, uint8_t *plaintext);

/**********************
 *   STATIC FUNCTIONS
 **********************/
static inline int aes_set_key(mbedtls_aes_context *ctx, const uint8_t *key)
{
	return mbedtls_aes_setkey_enc(ctx, key, 256);
}

static int aes_crypt_block(mbedtls_aes_context *ctx, int mode,
						   const uint8_t *input, uint8_t *output)
{
	return mbedtls_aes_crypt_ecb(ctx, mode, input, output);
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
int aes_crypt_ecb(const uint8_t *key, int mode,
				  const uint8_t input[16], uint8_t output[16])
{
	mbedtls_aes_context ctx;
	int ret;

	mbedtls_aes_init(&ctx);

	ret = aes_set_key(&ctx, key);
	if (ret)
		goto error;

	ret = aes_crypt_block(&ctx, mode, input, output);
error:
	mbedtls_aes_free(&ctx);

	return ret;
}

int aes_encrypt(const uint8_t key[32], const uint8_t *plaintext,
				size_t len, uint8_t *ciphertext)
{
	size_t i;
	int ret;

	if (len % AES_BLOCK_SIZE)
		return -EINVAL;

	for (i = 0; i < len; i += AES_BLOCK_SIZE)
	{
		ret = aes_crypt_ecb(key, MBEDTLS_AES_ENCRYPT,
							plaintext + i, ciphertext + i);
		if (ret)
			return ret;
	}

	return 0;
}

int aes_decrypt(const uint8_t key[32], const uint8_t *ciphertext,
				size_t len, uint8_t *plaintext)
{
	size_t i;
	int ret;

	if (len % AES_BLOCK_SIZE)
		return -EINVAL;

	for (i = 0; i < len; i += AES_BLOCK_SIZE)
	{
		ret = aes_crypt_ecb(key, MBEDTLS_AES_DECRYPT,
							ciphertext + i, plaintext + i);
		if (ret)
			return ret;
	}

	return 0;
}