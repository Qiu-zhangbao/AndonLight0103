/**********************************************************\
|                                                          |
| xxtea.c                                                  |
|                                                          |
| 可生成等长的秘文                                           |
|        
|                                                          |
\**********************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "wiced_memory.h"

//#include <sys/types.h> /* This will likely define BYTE_ORDER */

#define xxtea_F_malloc(size) wiced_bt_get_buffer(size)
#define xxtea_F_free(ptr) wiced_bt_free_buffer(ptr)


#define MX (((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (key[(p & 3) ^ e] ^ z))
#define DELTA 0x9e3779b9

#define FIXED_KEY                                     \
    int i;                                            \
    uint8_t fixed_key[16];                            \
    memcpy(fixed_key, key, 16);                       \
    for (i = 0; (i < 16) && (fixed_key[i] != 0); ++i) \
        ;                                             \
    for (++i; i < 16; ++i)                            \
        fixed_key[i] = 0;

static inline uint8_t* xxtea_F_calloc(uint16_t len, uint16_t size)
{
    uint8_t *data;
    data = xxtea_F_malloc(len*size);
    if(data != NULL)
    {
        memset(data,0,len*size);
    }
    return data;
}
static uint32_t *xxtea_F_to_uint_array(const uint8_t *data, int len, int inc_len, int *out_len )
{
    uint32_t *out;
#if !(defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN))
    int i;
#endif
    int n;

    n = (((len & 3) == 0) ? (len >> 2) : ((len >> 2) + 1));

    if (inc_len) {
        out = (uint32_t *)xxtea_F_calloc(n + 1, sizeof(uint32_t));
        if (!out){
            return NULL;
        }
        out[n]   = (uint32_t)len;
        *out_len = n+1;
    }
    else {
        out = (uint32_t *)xxtea_F_calloc(n, sizeof(uint32_t));
        if (!out){
            return NULL;
        }
        *out_len = n;
    }
#if defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
    memcpy(out, data, len);
#else
    for (i = 0; i < len; ++i) {
        out[i >> 2] |= (uint32_t)data[i] << ((i & 3) << 3);
    }
#endif

    return out;
}

static uint8_t *xxtea_F_to_ubyte_array(const uint32_t *data, int len, int inc_len, int *out_len)
{

    uint8_t *out = NULL;
#if !(defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN))
    int i;
#endif
    int m, n;

    n = len << 2;

    if (inc_len) {
        m = data[len - 1];
        n -= 4;
        if ((m < n - 3) || (m > n)) {
            return NULL;
        }
        n = m;
        out = (uint8_t *)xxtea_F_malloc(n+1);
        out[n] = '\0';
    }
    else
    {
        out = (uint8_t *)xxtea_F_malloc(n);
    }
    
    if (NULL == out){
            return NULL;
        }
#if defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
    memcpy(out, data, n);
#else
    for (i = 0; i < n; ++i) {
        out[i] = (uint8_t)(data[i >> 2] >> ((i & 3) << 3));
    }
#endif
    *out_len = n;

    return out;
}

static uint32_t *xxtea_F_uint_encrypt(uint32_t *data, int len, uint32_t *key)
{
    uint32_t n = (uint32_t)len - 1;
    uint32_t z = data[n], y = 0, p = 0, q = 6 + 52 / (n + 1), sum = 0, e = 0;

    if (n < 1)
        return data;

    while (0 < q--) {
        sum += DELTA;
        e = sum >> 2 & 3;

        for (p = 0; p < n; p++) {
            y = data[p + 1];
            z = data[p] += MX;
        }

        y = data[0];
        z = data[n] += MX;
    }

    return data;
}

static uint32_t *xxtea_F_uint_decrypt(uint32_t *data, int len, uint32_t *key)
{
    uint32_t n = (uint32_t)len - 1;
    uint32_t z, y = data[0], p, q = 6 + 52 / (n + 1), sum = q * DELTA, e;

    if (n < 1)
        return data;

    while (sum != 0) {
        e = sum >> 2 & 3;

        for (p = n; p > 0; p--) {
            z = data[p - 1];
            y = data[p] -= MX;
        }

        z = data[n];
        y = data[0] -= MX;
        sum -= DELTA;
    }

    return data;
}

static uint8_t *xxtea_F_ubyte_encrypt(const uint8_t *data, int len, const uint8_t *key, int *out_len, wiced_bool_t addlen)
{
    uint8_t *out;
    uint32_t *data_array, *key_array;
    int data_len, key_len;

    if (!len)
        return NULL;
    
    data_array = xxtea_F_to_uint_array(data, len, addlen, &data_len);
    if (!data_array)
        return NULL;
    
    key_array = xxtea_F_to_uint_array(key, 16, 0, &key_len);
    if (!key_array) {
        xxtea_F_free(data_array);
        return NULL;
    }

    out = xxtea_F_to_ubyte_array(xxtea_F_uint_encrypt(data_array, data_len, key_array), data_len, 0, out_len);
    
    xxtea_F_free(data_array);
    xxtea_F_free(key_array);

    return out;
}

static uint8_t *xxtea_F_ubyte_decrypt(const uint8_t *data, int len, const uint8_t *key, int *out_len, wiced_bool_t addlen)
{
    uint8_t *out;
    uint32_t *data_array, *key_array;
    int data_len, key_len;

    if (!len){
        return NULL;
    }
    
    data_array = xxtea_F_to_uint_array(data, len, 0, &data_len);
    if (!data_array){
        return NULL;
    }

    key_array = xxtea_F_to_uint_array(key, 16, 0, &key_len);
    if (!key_array) {
        xxtea_F_free(data_array);
        return NULL;
    }
    out = xxtea_F_to_ubyte_array(xxtea_F_uint_decrypt(data_array, data_len, key_array), data_len, addlen, out_len);
    xxtea_F_free(data_array);
    xxtea_F_free(key_array);
    return out;
}

// public functions

void * WYZE_F_xxtea_encrypt(unsigned char *data, int len, const void *key, size_t * out_len, wiced_bool_t addlen)
{
    FIXED_KEY
    return xxtea_F_ubyte_encrypt((const uint8_t *)data, len, fixed_key, out_len, addlen);
    
}

void * WYZE_F_xxtea_decrypt(unsigned char *data, int len, const void *key, size_t * out_len, wiced_bool_t addlen)
{
    FIXED_KEY
    return xxtea_F_ubyte_decrypt((const uint8_t *)data, len, fixed_key, out_len, addlen);
}
