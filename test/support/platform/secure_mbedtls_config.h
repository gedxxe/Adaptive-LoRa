// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECDSA_DETERMINISTIC
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_HKDF_C
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_MD_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_SHA256_C
#if defined(_WIN32)
// The repository's MinGW headers predate the SDK SecureZeroMemory inline.
#define MBEDTLS_PLATFORM_ZEROIZE_ALT
#endif
