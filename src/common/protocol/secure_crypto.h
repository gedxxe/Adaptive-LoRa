// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#if defined(ADAPTIVE_LORA_SECURE)
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include "secure_wire.h"

namespace adaptive_lora::secure {
constexpr size_t nonceBytes = 13;
using Entropy = int (*)(void*, unsigned char*, size_t);

bool hkdf(const uint8_t* key, size_t keySize, const uint8_t* salt, size_t saltSize,
          const uint8_t* info, size_t infoSize, uint8_t* output, size_t outputSize);
bool mac(const uint8_t* key, size_t keySize, const uint8_t* bytes, size_t length, uint8_t* tag);
bool equal(const uint8_t* a, const uint8_t* b, size_t length);
bool ccm(bool encrypt, const uint8_t* key, const uint8_t* nonce, const uint8_t* aad, size_t aadLen,
         const uint8_t* input, uint8_t* output, size_t length, uint8_t* tag, size_t tagSize = tagBytes);

class Crypto {
public:
    Crypto();
    ~Crypto();
    Crypto(const Crypto&) = delete;
    Crypto& operator=(const Crypto&) = delete;

    bool begin(bool gateway, const uint8_t* signingKey, Entropy entropy);
    bool random(uint8_t* output, size_t size);
    bool sign(const uint8_t* bytes, size_t length, uint8_t* signature);
    bool verify(const uint8_t* bytes, size_t length, const uint8_t* signature);

private:
    static int initialEntropy(void* context, unsigned char* output, size_t size);
    Entropy entropy_ = nullptr;
    bool ready_ = false;
    mbedtls_ctr_drbg_context random_;
    mbedtls_ecp_group group_;
    mbedtls_ecp_point public_;
    mbedtls_mpi private_;
};
}
#endif
