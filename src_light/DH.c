#include <stdio.h>
#include <stdlib.h>

#include "DH.h"
//#define PUBLIC_KEYS_G 5
/**
 * @description: 计算 a * b % PUBLIC_KEYS_P
 * @param: 两个随机数
 * @return: 
 */
static unsigned int mul_mod_p(unsigned int a, unsigned int b, unsigned int publishKey_P)
{
    unsigned int m = 0;
    while (b) {
        if (b & 1) {
            unsigned int t = publishKey_P - a;
            if (m >= t) {
                m -= t;
            }
            else {
                m += a;
            }
        }
        if (a >= publishKey_P - a) {
            a = a * 2 - publishKey_P;
        }
        else {
            a = a * 2;
        }
        b >>= 1;
    }
    return m;
}
/**
 * @description: 计算 a^b % publishKey_P
 * @param: 两个随机数
 * @return: 
 */
static unsigned int pow_mod_p(unsigned int a, unsigned int b, unsigned int publishKey_P)
{
    if (1 == b) {
        return a;
    }
    unsigned int t = pow_mod_p(a, b >> 1, publishKey_P);
    t              = mul_mod_p(t, t, publishKey_P);
    if (b % 2) {
        t = mul_mod_p(t, a, publishKey_P);
    }
    return t;
}
/**
 * @description: 计算 a^b % publishKey_P
 * @param: 两个随机数
 * @return: 
 */
unsigned int powmodp(unsigned int a, unsigned int b, unsigned int publishKey_P)
{
    if (a > publishKey_P)
        a %= publishKey_P;
    return pow_mod_p(a, b, publishKey_P);
}
/**
 * @description: 用于生成与通讯对方互换的key
 * @param: 无
 * @return: 结构体包含传到对端的key和
 */
unsigned int DHKeyGenerate(unsigned int random, unsigned int publishKey_P, unsigned int publishKey_G)
{
    return powmodp(publishKey_G, random, publishKey_P);
}
/**
 * @description: 用于生成与通讯对方互换的key
 * @param: 无
 * @return: 结构体包含传到对端的key和
 */
unsigned int DHSecretKeyGenerate(unsigned int random, unsigned int exchangeKey, unsigned int publishKey_P)
{
    return powmodp(exchangeKey, random, publishKey_P);
}