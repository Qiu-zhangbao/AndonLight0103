#ifndef WYZEFDK_BASICS_XXTEA_XXTEA_H
#define WYZEFDK_BASICS_XXTEA_XXTEA_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Function: WYZE_F_xxtea_encrypt
 * @data:    Data to be encrypted
 * @len:     Length of the data to be encrypted
 * @key:     Symmetric key
 * @out_len: Pointer to output length variable
 * @addlen:  加密数据包时是否包含长度
 * Returns:  Encrypted data or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
void * WYZE_F_xxtea_encrypt(unsigned char *data, int len, const void *key, size_t * out_len, wiced_bool_t addlen);
/**
 * Function: WYZE_F_xxtea_decrypt
 * @data:    Data to be decrypted
 * @len:     Length of the data to be decrypted
 * @key:     Symmetric key
 * @out_len: Pointer to output length variable
 * @addlen:  带解密数据包中是否包含长度
 * Returns:  Decrypted data or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
void * WYZE_F_xxtea_decrypt(unsigned char *data, int len, const void *key, size_t * out_len, wiced_bool_t addlen);

#ifdef __cplusplus
}
#endif

#endif
