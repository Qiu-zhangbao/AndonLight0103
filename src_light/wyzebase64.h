#ifndef WYZEFDK_BASICS_BASE64_LOCALBASE64_H
#define WYZEFDK_BASICS_BASE64_LOCALBASE64_H

#include <stdlib.h>
#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * Function: WYZE_base64_encode
 * @data:    Data to be encoded
 * @len:     Length of the data to be encoded
 * Returns:  Encoded data or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
char * WYZE_base64_encode(const unsigned char * data, uint16_t len);

/**
 * Function: WYZE_base64_decode
 * @data:    Data to be decoded
 * @out_len: Pointer to output length variable
 * Returns:  Decoded data or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
 unsigned char * WYZE_base64_decode(const char * data, uint16_t* out_len);

#ifdef  __cplusplus
}
#endif

#endif
