#ifndef _AES_H_
#define _AES_H_

#include <stdint.h>

struct AES_ctx {
  uint8_t RoundKey[176]; // expanded key
  uint8_t Iv[16];        // CTR IV
};
typedef struct AES_ctx AES_ctx;

#ifdef __cplusplus
extern "C" {
#endif

void AES_init_ctx_iv(AES_ctx* ctx, const uint8_t* key, const uint8_t* iv);
void AES_CTR_xcrypt_buffer(AES_ctx* ctx, uint8_t* buf, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif
