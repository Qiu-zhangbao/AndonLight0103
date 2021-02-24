#ifndef DH_H 
#define DH_H

#define PUBLIC_KEYS_P       0xffffffc5
#define PUBLIC_KEYS_G       5

unsigned int DHKeyGenerate(unsigned int random, unsigned int publishKey_P, unsigned int publishKey_G);
unsigned int DHSecretKeyGenerate(unsigned int random, unsigned int exchangeKey, unsigned int publishKey_P);
#endif