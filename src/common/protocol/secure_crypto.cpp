// SPDX-License-Identifier: GPL-3.0-only
#include "secure_crypto.h"
#if defined(ADAPTIVE_LORA_SECURE)
#include <climits>
#include <cstring>
#include <mbedtls/ccm.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>
extern "C" {
#include <mbedtls/constant_time.h>
}

namespace adaptive_lora::secure {
bool hkdf(const uint8_t* key, size_t keySize, const uint8_t* salt, size_t saltSize,
          const uint8_t* info, size_t infoSize, uint8_t* output, size_t outputSize) {
    return mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt, saltSize,
                        key, keySize, info, infoSize, output, outputSize) == 0;
}

bool mac(const uint8_t* key, size_t keySize, const uint8_t* bytes, size_t length, uint8_t* tag) {
    uint8_t digest[32]{};
    const int result = mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                                       key, keySize, bytes, length, digest);
    if (!result) std::memcpy(tag, digest, tagBytes);
    mbedtls_platform_zeroize(digest, sizeof(digest));
    return !result;
}

bool equal(const uint8_t* a, const uint8_t* b, size_t length) {
    return mbedtls_ct_memcmp(a, b, length) == 0;
}

bool ccm(bool encrypt, const uint8_t* key, const uint8_t* nonce, const uint8_t* aad, size_t aadLen,
         const uint8_t* input, uint8_t* output, size_t length, uint8_t* tag, size_t tagSize) {
    mbedtls_ccm_context context;
    mbedtls_ccm_init(&context);
    int result = mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key, keyBytes * CHAR_BIT);
    if (!result) {
        result = encrypt ?
            mbedtls_ccm_encrypt_and_tag(&context, length, nonce, nonceBytes, aad, aadLen, input, output, tag, tagSize) :
            mbedtls_ccm_auth_decrypt(&context, length, nonce, nonceBytes, aad, aadLen, input, output, tag, tagSize);
    }
    mbedtls_ccm_free(&context);
    return !result;
}

Crypto::Crypto() {
    mbedtls_ctr_drbg_init(&random_);
    mbedtls_ecp_group_init(&group_);
    mbedtls_ecp_point_init(&public_);
    mbedtls_mpi_init(&private_);
}

Crypto::~Crypto() {
    mbedtls_mpi_free(&private_);
    mbedtls_ecp_point_free(&public_);
    mbedtls_ecp_group_free(&group_);
    mbedtls_ctr_drbg_free(&random_);
}

int Crypto::initialEntropy(void* context, unsigned char* output, size_t size) {
    auto entropy = static_cast<Crypto*>(context)->entropy_;
    return entropy ? entropy(nullptr, output, size) : MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
}

bool Crypto::begin(bool gateway, const uint8_t* signingKey, Entropy entropy) {
    if (ready_ || !entropy || mbedtls_ecp_group_load(&group_, MBEDTLS_ECP_DP_SECP256R1)) return false;
    if (gateway) {
        if (mbedtls_mpi_read_binary(&private_, signingKey, privateKeyBytes) ||
            mbedtls_ecp_check_privkey(&group_, &private_)) return false;
    } else if (mbedtls_ecp_point_read_binary(&group_, &public_, signingKey, publicKeyBytes) ||
               mbedtls_ecp_check_pubkey(&group_, &public_)) return false;
    constexpr uint8_t personalization[] = "Adaptive-LoRa v7";
    entropy_ = entropy;
    ready_ = mbedtls_ctr_drbg_seed(&random_, initialEntropy, this,
                                   personalization, sizeof(personalization) - 1) == 0;
    entropy_ = nullptr; // A runtime reseed without startup entropy must fail.
    mbedtls_ctr_drbg_set_reseed_interval(&random_, INT_MAX);
    return ready_;
}

bool Crypto::random(uint8_t* output, size_t size) {
    return ready_ && mbedtls_ctr_drbg_random(&random_, output, size) == 0;
}

bool Crypto::sign(const uint8_t* bytes, size_t length, uint8_t* signature) {
    if (!ready_) return false;
    uint8_t digest[32];
    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    const bool ok = !mbedtls_sha256(bytes, length, digest, 0) &&
        !mbedtls_ecdsa_sign_det_ext(&group_, &r, &s, &private_, digest, sizeof(digest), MBEDTLS_MD_SHA256,
                                    mbedtls_ctr_drbg_random, &random_) &&
        !mbedtls_mpi_write_binary(&r, signature, privateKeyBytes) &&
        !mbedtls_mpi_write_binary(&s, signature + privateKeyBytes, privateKeyBytes);
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    return ok;
}

bool Crypto::verify(const uint8_t* bytes, size_t length, const uint8_t* signature) {
    if (!ready_) return false;
    uint8_t digest[32];
    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    const bool ok = !mbedtls_sha256(bytes, length, digest, 0) &&
        !mbedtls_mpi_read_binary(&r, signature, privateKeyBytes) &&
        !mbedtls_mpi_read_binary(&s, signature + privateKeyBytes, privateKeyBytes) &&
        !mbedtls_ecdsa_verify(&group_, digest, sizeof(digest), &public_, &r, &s);
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    return ok;
}
}
#endif
