# SPDX-License-Identifier: GPL-3.0-only
"""Compile the installed ESP-IDF Mbed TLS sources into the native security tests."""
import os
from pathlib import Path

Import("env")  # type: ignore[name-defined]  # PlatformIO SCons hook.

core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
installed = core / "packages/framework-espidf/components/mbedtls/mbedtls"
mbed = Path(os.environ.get("ADAPTIVE_LORA_MBEDTLS_DIR", installed))
version = mbed / "include/mbedtls/build_info.h"
if not version.is_file() or '"3.6.6"' not in version.read_text(encoding="utf-8"):
    raise RuntimeError("native_secure requires Mbed TLS 3.6.6 sources (set ADAPTIVE_LORA_MBEDTLS_DIR)")

support = Path(env.subst("$PROJECT_DIR")) / "test/support/platform"
env.Append(CPPPATH=[str(mbed / "include"), str(mbed / "library"), str(support)])
env.Append(CPPDEFINES=[("MBEDTLS_CONFIG_FILE", r'\"secure_mbedtls_config.h\"')])
crypto = env.Clone()
crypto.Replace(CC=os.environ.get("CC", "gcc"))
crypto.ProcessUnFlags(["-std=c++17"])
sources = (
    "aes", "aesni", "asn1parse", "asn1write", "bignum", "bignum_core", "bignum_mod",
    "bignum_mod_raw", "ccm", "cipher", "cipher_wrap", "constant_time", "ctr_drbg",
    "ecdsa", "ecp", "ecp_curves", "ecp_curves_new", "hkdf", "hmac_drbg",
    "md", "platform", "platform_util", "sha256",
)
crypto.BuildSources("$BUILD_DIR/mbedtls", str(mbed / "library"),
                    src_filter=["-<*>"] + [f"+<{source}.c>" for source in sources])
for source in sources:
    crypto.Depends(f"$BUILD_DIR/mbedtls/{source}.o", str(support / "secure_mbedtls_config.h"))
