#!/bin/bash
# rebuild_mbedtls.sh — recompile mbedtls for 68K Mac and update the static archives.
#
# Run this after any change to:
#   - <mbedtls-Mac-68k>/include/mbedtls/mbedtls_config.h
#   - <mbedtls-Mac-68k>/library/*.c
#
# After running this script, rebuild the app:
#   cd <project>/build && make -j4
#
# Set these to match your local checkout paths:

set -e

CC="${RETRO68_DIR:-/path/to/Retro68}/Retro68-build/toolchain/bin/m68k-apple-macos-gcc"
AR="${RETRO68_DIR:-/path/to/Retro68}/Retro68-build/toolchain/bin/m68k-apple-macos-ar"
SRCDIR="${MBEDTLS_MAC68K_DIR:-/path/to/mbedtls-Mac-68k}/library"
OUTDIR="${MBEDTLS_MAC68K_DIR:-/path/to/mbedtls-Mac-68k}/build/library"
CFLAGS="-m68020 -Ofast -ffunction-sections -I${SRCDIR} -I${SRCDIR}/../include -D_FILE_OFFSET_BITS=64"

TMPDIR=$(mktemp -d)
echo "Compiling to $TMPDIR ..."

compile() {
    local src="$1"
    local out="$2"
    echo "  CC $src"
    $CC $CFLAGS -c "$SRCDIR/$src" -o "$TMPDIR/$out"
}

# libmbedcrypto
compile aes.c aes.o
compile asn1parse.c asn1parse.o
compile asn1write.c asn1write.o
compile base64.c base64.o
compile bignum.c bignum.o
compile cipher.c cipher.o
compile cipher_wrap.c cipher_wrap.o
# cmac.c       — removed: MBEDTLS_CMAC_C disabled
compile constant_time.c constant_time.o
compile ctr_drbg.c ctr_drbg.o
# des.c        — removed: MBEDTLS_DES_C disabled
# dhm.c        — removed: MBEDTLS_DHM_C disabled (no DHE-RSA/DHE-PSK suites)
compile ecdh.c ecdh.o
compile ecdsa.c ecdsa.o
# ecjpake.c    — removed: MBEDTLS_ECJPAKE_C disabled
compile ecp.c ecp.o
compile ecp_curves.c ecp_curves.o
compile entropy.c entropy.o
compile gcm.c gcm.o
# hkdf.c       — removed: MBEDTLS_HKDF_C disabled (TLS 1.3 only)
# hmac_drbg.c  — removed: MBEDTLS_HMAC_DRBG_C disabled (using CTR_DRBG)
compile md.c md.o
compile md5.c md5.o
# nist_kw.c    — removed: MBEDTLS_NIST_KW_C disabled
compile oid.c oid.o
compile pem.c pem.o
compile pk.c pk.o
compile pk_wrap.c pk_wrap.o
# pkcs5.c      — removed: MBEDTLS_PKCS5_C disabled
# pkcs12.c     — removed: MBEDTLS_PKCS12_C disabled
compile pkparse.c pkparse.o
# pkwrite.c    — removed: MBEDTLS_PK_WRITE_C disabled (client never writes keys)
compile platform.c platform.o
compile platform_util.c platform_util.o
compile poly1305.c poly1305.o
compile psa_stubs.c psa_stubs.o
# ripemd160.c  — removed: MBEDTLS_RIPEMD160_C disabled
compile rsa.c rsa.o
compile rsa_alt_helpers.c rsa_alt_helpers.o
compile sha1.c sha1.o
compile sha256.c sha256.o
compile sha512.c sha512.o
compile threading.c threading.o
compile version.c version.o
# aria.c       — removed: MBEDTLS_ARIA_C disabled
# camellia.c   — removed: MBEDTLS_CAMELLIA_C disabled
# ccm.c        — removed: MBEDTLS_CCM_C disabled
compile chacha20.c chacha20.o
compile chachapoly.c chachapoly.o

# platform_util_new is a second copy of platform_util (kept for --allow-multiple-definition)
cp "$TMPDIR/platform_util.o" "$TMPDIR/platform_util_new.o"

# uECC and uecc_alt use special -m68020 + optimization flags — keep existing compiled objects
cp "$OUTDIR/uECC.o" "$TMPDIR/uECC.o"
cp "$OUTDIR/uecc_alt.o" "$TMPDIR/uecc_alt.o"

# libmbedtls (SSL/TLS)
# debug.c      — removed: MBEDTLS_DEBUG_C disabled
# ssl_cache.c  — removed: MBEDTLS_SSL_CACHE_C disabled (server feature)
compile ssl_ciphersuites.c ssl_ciphersuites.o
compile ssl_client.c ssl_client.o
# ssl_cookie.c — removed: MBEDTLS_SSL_COOKIE_C disabled (server feature)
compile ssl_msg.c ssl_msg.o
# ssl_ticket.c — removed: MBEDTLS_SSL_TICKET_C disabled (server feature)
compile ssl_tls12_client.c ssl_tls12_client.o
# ssl_tls12_server.c — removed: MBEDTLS_SSL_SRV_C disabled (client only)
compile ssl_debug_helpers_generated.c ssl_debug_helpers_generated.o
compile ssl_tls.c ssl_tls_new.o   # archived as ssl_tls_new.o

# libmbedx509
compile x509.c x509.o
# x509_create.c  — removed: MBEDTLS_X509_CREATE_C disabled
# x509_crl.c     — removed: MBEDTLS_X509_CRL_PARSE_C disabled
compile x509_crt.c x509_crt.o
# x509_csr.c     — removed: MBEDTLS_X509_CSR_PARSE_C disabled
# x509write_crt.c — removed: MBEDTLS_X509_CRT_WRITE_C disabled
# x509write_csr.c — removed: MBEDTLS_X509_CSR_WRITE_C disabled

echo "Archiving libmbedcrypto.a ..."
$AR rcs "$OUTDIR/libmbedcrypto.a" \
    "$TMPDIR/aes.o" "$TMPDIR/asn1parse.o" "$TMPDIR/asn1write.o" "$TMPDIR/base64.o" \
    "$TMPDIR/bignum.o" "$TMPDIR/cipher.o" "$TMPDIR/cipher_wrap.o" \
    "$TMPDIR/constant_time.o" "$TMPDIR/ctr_drbg.o" \
    "$TMPDIR/ecdh.o" "$TMPDIR/ecdsa.o" "$TMPDIR/ecp.o" \
    "$TMPDIR/ecp_curves.o" "$TMPDIR/entropy.o" "$TMPDIR/gcm.o" \
    "$TMPDIR/md.o" "$TMPDIR/md5.o" \
    "$TMPDIR/oid.o" "$TMPDIR/pem.o" "$TMPDIR/pk.o" "$TMPDIR/pk_wrap.o" \
    "$TMPDIR/pkparse.o" \
    "$TMPDIR/platform.o" "$TMPDIR/platform_util.o" "$TMPDIR/platform_util_new.o" \
    "$TMPDIR/poly1305.o" "$TMPDIR/psa_stubs.o" \
    "$TMPDIR/rsa.o" "$TMPDIR/rsa_alt_helpers.o" "$TMPDIR/sha1.o" \
    "$TMPDIR/sha256.o" "$TMPDIR/sha512.o" "$TMPDIR/threading.o" "$TMPDIR/version.o" \
    "$TMPDIR/chacha20.o" "$TMPDIR/chachapoly.o" \
    "$TMPDIR/uECC.o" "$TMPDIR/uecc_alt.o"

echo "Archiving libmbedtls.a ..."
$AR rcs "$OUTDIR/libmbedtls.a" \
    "$TMPDIR/ssl_ciphersuites.o" \
    "$TMPDIR/ssl_client.o" "$TMPDIR/ssl_msg.o" \
    "$TMPDIR/ssl_tls12_client.o" \
    "$TMPDIR/ssl_debug_helpers_generated.o" "$TMPDIR/ssl_tls_new.o"

echo "Archiving libmbedx509.a ..."
$AR rcs "$OUTDIR/libmbedx509.a" \
    "$TMPDIR/x509.o" "$TMPDIR/x509_crt.o"

rm -rf "$TMPDIR"
echo "Done. Run: cd build && make -j4"
