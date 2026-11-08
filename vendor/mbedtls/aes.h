#pragma once
#include "../aes.h"
#include <string.h>
typedef struct AES_ctx mbedtls_aes_context;
#define MBEDTLS_AES_ENCRYPT 1
#define MBEDTLS_AES_DECRYPT 0
static inline void mbedtls_aes_init(mbedtls_aes_context *c){memset(c,0,sizeof *c);}
static inline int mbedtls_aes_setkey_enc(mbedtls_aes_context *c,const unsigned char *k,unsigned bits){(void)bits;AES_init_ctx(c,k);return 0;}
static inline int mbedtls_aes_setkey_dec(mbedtls_aes_context *c,const unsigned char *k,unsigned bits){return mbedtls_aes_setkey_enc(c,k,bits);}
static inline int mbedtls_aes_crypt_ecb(mbedtls_aes_context *c,int mode,const unsigned char *in,unsigned char *out){memmove(out,in,16);if(mode)AES_ECB_encrypt(c,out);else AES_ECB_decrypt(c,out);return 0;}
