/*
 * Copyright (c) 2019, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nsconfig.h"

#ifdef HAVE_WS
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_SSL_TLS_C) && defined(MBEDTLS_X509_CRT_PARSE_C) && defined(MBEDTLS_SSL_EXPORT_KEYS) /* EXPORT_KEYS not supported by mbedtls baremetal yet */
#define WS_MBEDTLS_SECURITY_ENABLED
#endif

#include <string.h>
#include <randLIB.h>
#include "platform/arm_hal_random.h"
#include "ns_types.h"
#include "ns_list.h"
#include "ns_trace.h"
#include "nsdynmemLIB.h"
#include "common_functions.h"
#include "Service_Libs/Trickle/trickle.h"
#include "Security/protocols/sec_prot_cfg.h"
#include "Security/protocols/sec_prot_certs.h"
#include "Security/protocols/tls_sec_prot/tls_sec_prot_lib.h"
#include "swo_debug.h"

#if defined(MBEDTLS_SSL_TLS_C) && defined(MBEDTLS_X509_CRT_PARSE_C) && defined(MBEDTLS_SSL_EXPORT_KEYS) /* EXPORT_KEYS not supported by mbedtls baremetal yet */
#ifdef WS_MBEDTLS_SECURITY_ENABLED
#endif
//KV code coming here
#include "mbedtls/sha256.h"
#include "mbedtls/error.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl_cookie.h"
#include "mbedtls/entropy.h"
#include "mbedtls/entropy_poll.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ssl_ciphersuites.h"
#include "mbedtls/debug.h"
#include "mbedtls/oid.h"

#define TRACE_GROUP "tlsl"

#define TLS_HANDSHAKE_TIMEOUT_MIN 25000
#define TLS_HANDSHAKE_TIMEOUT_MAX 201000

//#define TLS_SEC_PROT_LIB_TLS_DEBUG       // Enable mbed TLS debug traces

typedef int tls_sec_prot_lib_crt_verify_cb(tls_security_t *sec, mbedtls_x509_crt *crt, uint32_t *flags);

struct tls_security_s {
    mbedtls_ssl_config             conf;                 /**< mbed TLS SSL configuration */
    mbedtls_ssl_context            ssl;                  /**< mbed TLS SSL context */

    mbedtls_ctr_drbg_context       ctr_drbg;             /**< mbed TLS pseudo random number generator context */
    mbedtls_entropy_context        entropy;              /**< mbed TLS entropy context */

    mbedtls_x509_crt               cacert;               /**< CA certificate(s) */
    mbedtls_x509_crl               *crl;                 /**< Certificate Revocation List */
    mbedtls_x509_crt               owncert;              /**< Own certificate(s) */
    mbedtls_pk_context             pkey;                 /**< Private key for own certificate */
    void                           *handle;              /**< Handle provided in callbacks (defined by library user) */
    bool                           ext_cert_valid : 1;   /**< Extended certificate validation enabled */
    tls_sec_prot_lib_crt_verify_cb *crt_verify;          /**< Verify function for client/server certificate */
    tls_sec_prot_lib_send          *send;                /**< Send callback */
    tls_sec_prot_lib_receive       *receive;             /**< Receive callback */
    tls_sec_prot_lib_export_keys   *export_keys;         /**< Export keys callback */
    tls_sec_prot_lib_set_timer     *set_timer;           /**< Set timer callback */
    tls_sec_prot_lib_get_timer     *get_timer;           /**< Get timer callback */
};

static void tls_sec_prot_lib_ssl_set_timer(void *ctx, uint32_t int_ms, uint32_t fin_ms);
static int tls_sec_prot_lib_ssl_get_timer(void *ctx);
static int tls_sec_lib_entropy_poll(void *data, unsigned char *output, size_t len, size_t *olen);
static int tls_sec_prot_lib_ssl_send(void *ctx, const unsigned char *buf, size_t len);
static int tls_sec_prot_lib_ssl_recv(void *ctx, unsigned char *buf, size_t len);
static int tls_sec_prot_lib_ssl_export_keys(void *p_expkey, const unsigned char *ms,
                                            const unsigned char *kb, size_t maclen, size_t keylen,
                                            size_t ivlen, const unsigned char client_random[32],
                                            const unsigned char server_random[32],
                                            mbedtls_tls_prf_types tls_prf_type);
static int tls_sec_lib_seed_poll(void *ctx, unsigned char *output, size_t len);
static uint32_t tls_sec_lib_debug_random_word(void);

static int tls_sec_prot_lib_x509_crt_verify(void *ctx, mbedtls_x509_crt *crt, int certificate_depth, uint32_t *flags);
static int8_t tls_sec_prot_lib_subject_alternative_name_validate(mbedtls_x509_crt *crt);
static int8_t tls_sec_prot_lib_extended_key_usage_validate(mbedtls_x509_crt *crt);
#ifdef HAVE_PAE_AUTH
static int tls_sec_prot_lib_x509_crt_idevid_ldevid_verify(tls_security_t *sec, mbedtls_x509_crt *crt, uint32_t *flags);
#endif
#ifdef HAVE_PAE_SUPP
static int tls_sec_prot_lib_x509_crt_server_verify(tls_security_t *sec, mbedtls_x509_crt *crt, uint32_t *flags);
#endif
#ifdef TLS_SEC_PROT_LIB_TLS_DEBUG
static void tls_sec_prot_lib_debug(void *ctx, int level, const char *file, int line, const char *string);
#endif
#ifdef MBEDTLS_PLATFORM_MEMORY
// Disable for now
//#define TLS_SEC_PROT_LIB_USE_MBEDTLS_PLATFORM_MEMORY
#endif
#ifdef TLS_SEC_PROT_LIB_USE_MBEDTLS_PLATFORM_MEMORY
static void *tls_sec_prot_lib_mem_calloc(size_t count, size_t size);
static void tls_sec_prot_lib_mem_free(void *ptr);
#endif

#if defined(HAVE_PAE_AUTH) && defined(HAVE_PAE_SUPP)
#define is_server_is_set (is_server == true)
#define is_server_is_not_set (is_server == false)
#elif defined(HAVE_PAE_AUTH)
#define is_server_is_set true
#define is_server_is_not_set false
#elif defined(HAVE_PAE_SUPP)
#define is_server_is_set false
#define is_server_is_not_set true
#endif

int8_t tls_sec_prot_lib_init(tls_security_t *sec)
{
    const char *pers = "ws_tls";
    int ret;

    (void)swoDebugPrintf("PYRONET_TLS_LIB_INIT_ENTER sec=%p", (void *)sec);

#ifdef TLS_SEC_PROT_LIB_USE_MBEDTLS_PLATFORM_MEMORY
    mbedtls_platform_set_calloc_free(tls_sec_prot_lib_mem_calloc, tls_sec_prot_lib_mem_free);
#endif

    (void)swoDebugPrintf("PYRONET_TLS_LIB_INIT_STEP step=ssl_init");
    mbedtls_ssl_init(&sec->ssl);
    mbedtls_ssl_config_init(&sec->conf);
    mbedtls_ctr_drbg_init(&sec->ctr_drbg);
    mbedtls_entropy_init(&sec->entropy);

    mbedtls_x509_crt_init(&sec->cacert);

    mbedtls_x509_crt_init(&sec->owncert);
    mbedtls_pk_init(&sec->pkey);

    sec->crl = NULL;

    (void)swoDebugPrintf("PYRONET_TLS_LIB_INIT_STEP step=direct_seed_config");
    mbedtls_ctr_drbg_set_entropy_len(&sec->ctr_drbg, 48);
    ret = mbedtls_ctr_drbg_set_nonce_len(&sec->ctr_drbg, 0);
    (void)swoDebugPrintf("PYRONET_TLS_LIB_INIT_RET step=nonce_len ret=%d", ret);
    if (ret < 0) {
        tr_error("drbg nonce config fail");
        return -1;
    }

    (void)swoDebugWriteLine("PYRONET_TLS_SEED_CALL");
    ret = mbedtls_ctr_drbg_seed(&sec->ctr_drbg, tls_sec_lib_seed_poll, NULL,
                                (const unsigned char *) pers, strlen(pers));
    (void)swoDebugPrintf("PYRONET_TLS_LIB_INIT_RET step=ctr_drbg_seed ret=%d", ret);
    if (ret != 0) {
        tr_error("drbg seed fail");
        return -1;
    }

    (void)swoDebugPrintf("PYRONET_TLS_LIB_INIT_DONE");
    return 0;
}

uint16_t tls_sec_prot_lib_size(void)
{
    return sizeof(tls_security_t);
}

void tls_sec_prot_lib_set_cb_register(tls_security_t *sec, void *handle,
                                      tls_sec_prot_lib_send *send, tls_sec_prot_lib_receive *receive,
                                      tls_sec_prot_lib_export_keys *export_keys, tls_sec_prot_lib_set_timer *set_timer,
                                      tls_sec_prot_lib_get_timer *get_timer)
{
    if (!sec) {
        return;
    }

    sec->handle = handle;
    sec->send = send;
    sec->receive = receive;
    sec->export_keys = export_keys;
    sec->set_timer = set_timer;
    sec->get_timer = get_timer;
}

void tls_sec_prot_lib_free(tls_security_t *sec)
{
    mbedtls_x509_crt_free(&sec->cacert);
    if (sec->crl) {
        mbedtls_x509_crl_free(sec->crl);
        ns_dyn_mem_free(sec->crl);
    }
    mbedtls_x509_crt_free(&sec->owncert);
    mbedtls_pk_free(&sec->pkey);
    mbedtls_entropy_free(&sec->entropy);
    mbedtls_ctr_drbg_free(&sec->ctr_drbg);
    mbedtls_ssl_config_free(&sec->conf);
    mbedtls_ssl_free(&sec->ssl);
}

static int tls_sec_prot_lib_configure_certificates(tls_security_t *sec, const sec_prot_certs_t *certs)
{
    (void)swoDebugPrintf("PYRONET_TLS_CERTS_START certs=%p own0=%p own_len0=%u key=%p key_len=%u own_chain_len=%u",
                         (void *)certs,
                         certs ? (void *)certs->own_cert_chain.cert[0] : NULL,
                         certs ? (unsigned int)certs->own_cert_chain.cert_len[0] : 0U,
                         certs ? (void *)certs->own_cert_chain.key : NULL,
                         certs ? (unsigned int)certs->own_cert_chain.key_len : 0U,
                         certs ? (unsigned int)certs->own_cert_chain_len : 0U);

    if (!certs->own_cert_chain.cert[0]) {
        tr_error("no own cert");
        (void)swoDebugPrintf("PYRONET_TLS_CERTS_FAIL reason=no_own_cert");
        return -1;
    }

    // Parse own certificate chain
    uint8_t index = 0;
    while (true) {
        uint16_t cert_len;
        uint8_t *cert = sec_prot_certs_cert_get(&certs->own_cert_chain, index, &cert_len);
        if (!cert) {
            if (index == 0) {
                tr_error("No own cert");
                return -1;
            }
            break;
        }
        (void)swoDebugPrintf("PYRONET_TLS_CERTS_PARSE_OWN index=%u len=%u", (unsigned int)index, (unsigned int)cert_len);
        int cert_ret = mbedtls_x509_crt_parse(&sec->owncert, cert, cert_len);
        (void)swoDebugPrintf("PYRONET_TLS_CERTS_PARSE_OWN_RET index=%u ret=%d", (unsigned int)index, cert_ret);
        if (cert_ret < 0) {
            tr_error("Own cert parse eror");
            return -1;
        }
        index++;
    }

    // Parse private key
    uint8_t key_len;
    uint8_t *key = sec_prot_certs_priv_key_get(&certs->own_cert_chain, &key_len);
    if (!key) {
        tr_error("No private key");
        (void)swoDebugPrintf("PYRONET_TLS_CERTS_FAIL reason=no_private_key");
        return -1;
    }

    (void)swoDebugPrintf("PYRONET_TLS_CERTS_PARSE_KEY len=%u", (unsigned int)key_len);
    int key_ret = mbedtls_pk_parse_key(&sec->pkey, key, key_len, NULL, 0);
    (void)swoDebugPrintf("PYRONET_TLS_CERTS_PARSE_KEY_RET ret=%d", key_ret);
    if (key_ret < 0) {
        tr_error("Private key parse error");
        return -1;
    }

    // Configure own certificate chain and private key
    int own_conf_ret = mbedtls_ssl_conf_own_cert(&sec->conf, &sec->owncert, &sec->pkey);
    (void)swoDebugPrintf("PYRONET_TLS_CERTS_OWN_CONF_RET ret=%d", own_conf_ret);
    if (own_conf_ret != 0) {
        tr_error("Own cert and private key conf error");
        return -1;
    }

    // Parse trusted certificate chains
#ifdef FEATURE_WISUN_SUPPORT
    bool ca_cert_initialized = false;
    struct mbedtls_x509_crt *next_ca_cert_link = NULL;
#endif
    ns_list_foreach(cert_chain_entry_t, entry, &certs->trusted_cert_chain_list) {
        index = 0;
        while (true) {
            uint16_t cert_len;
            uint8_t *cert = sec_prot_certs_cert_get(entry, index, &cert_len);
            if (!cert) {
                if (index == 0) {
                    tr_error("No trusted cert");
                    return -1;
                }
                break;
            }
#ifndef FEATURE_WISUN_SUPPORT
            if (mbedtls_x509_crt_parse(&sec->cacert, cert, cert_len) < 0) {
                tr_error("Trusted cert parse error");
                return -1;
            }
#else

                if(!ca_cert_initialized)
            {
                (void)swoDebugPrintf("PYRONET_TLS_CERTS_PARSE_TRUSTED index=%u len=%u", (unsigned int)index, (unsigned int)cert_len);
                int ca_ret = mbedtls_x509_crt_parse(&sec->cacert, cert, cert_len);
                (void)swoDebugPrintf("PYRONET_TLS_CERTS_PARSE_TRUSTED_RET index=%u ret=%d", (unsigned int)index, ca_ret);
                if (ca_ret < 0) {
                    tr_error("Trusted cert parse error");
                    return -1;
                }
                ca_cert_initialized = true;
                next_ca_cert_link = &sec->cacert;
            }
            else
            {
                struct mbedtls_x509_crt *next_ca_cert;
                next_ca_cert = ns_dyn_mem_alloc(sizeof(struct mbedtls_x509_crt));
                if(next_ca_cert != NULL)
                {
                    mbedtls_x509_crt_init(next_ca_cert);
                    if (mbedtls_x509_crt_parse(next_ca_cert, cert, cert_len) < 0) {
                                        tr_error("Trusted cert parse error");
                                        return -1;
                    }
                    if(next_ca_cert_link)
                    {
                        next_ca_cert_link->next = next_ca_cert;
                        // Link the certificate to end of list
                        next_ca_cert_link = next_ca_cert;
                        // Link the certificate to end of list
                    }
                }

            }

#endif
            index++;
        }
    }

    // Parse certificate revocation lists
    ns_list_foreach(cert_revocat_list_entry_t, entry, &certs->cert_revocat_lists) {
        uint16_t crl_len;
        const uint8_t *crl = sec_prot_certs_revocat_list_get(entry, &crl_len);
        if (!crl) {
            break;
        }
        if (!sec->crl) {
            sec->crl = ns_dyn_mem_temporary_alloc(sizeof(mbedtls_x509_crl));
            if (!sec->crl) {
                tr_error("No memory for CRL");
                return -1;
            }
            mbedtls_x509_crl_init(sec->crl);
        }

        if (mbedtls_x509_crl_parse(sec->crl, crl, crl_len) < 0) {
            tr_error("CRL parse error");
            return -1;
        }
    }

    // Configure trusted certificates and certificate revocation lists
    mbedtls_ssl_conf_ca_chain(&sec->conf, &sec->cacert, sec->crl);

    // Certificate verify required on both client and server
    mbedtls_ssl_conf_authmode(&sec->conf, MBEDTLS_SSL_VERIFY_REQUIRED);

    // Get extended certificate validation setting
    sec->ext_cert_valid = sec_prot_certs_ext_certificate_validation_get(certs);

    (void)swoDebugPrintf("PYRONET_TLS_CERTS_DONE ext_valid=%u",
                         (unsigned int)sec->ext_cert_valid);
    return 0;
}

int8_t tls_sec_prot_lib_connect(tls_security_t *sec, bool is_server, const sec_prot_certs_t *certs)
{
    int ret;

    (void)swoDebugPrintf("PYRONET_TLS_CONNECT_ENTER sec=%p role=%s certs=%p",
                         (void *)sec,
                         is_server ? "server" : "client",
                         (void *)certs);

#if !defined(HAVE_PAE_SUPP) || !defined(HAVE_PAE_AUTH)
    (void) is_server;
#endif

    if (!sec) {
        (void)swoDebugPrintf("PYRONET_TLS_CONNECT_FAIL reason=no_sec");
        return -1;
    }

#ifdef HAVE_PAE_SUPP
    if (is_server_is_not_set) {
        sec->crt_verify = tls_sec_prot_lib_x509_crt_server_verify;
    }
#endif
#ifdef HAVE_PAE_AUTH
    if (is_server_is_set) {
        sec->crt_verify = tls_sec_prot_lib_x509_crt_idevid_ldevid_verify;
    }
#endif


    (void)swoDebugPrintf("PYRONET_TLS_CONNECT_STEP step=config_defaults");
    ret = mbedtls_ssl_config_defaults(&sec->conf,
                                      is_server_is_set ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM, 0);
    (void)swoDebugPrintf("PYRONET_TLS_CONNECT_RET step=config_defaults ret=%d", ret);
    if (ret != 0) {
        tr_error("config defaults fail");
        return -1;
    }

#if !defined(MBEDTLS_SSL_CONF_RNG)
    // Configure random number generator
    mbedtls_ssl_conf_rng(&sec->conf, mbedtls_ctr_drbg_random, &sec->ctr_drbg);
#endif

#ifdef MBEDTLS_ECP_RESTARTABLE
    // Set ECC calculation maximum operations (affects only client)
    mbedtls_ecp_set_max_ops(ECC_CALCULATION_MAX_OPS);
#endif

    (void)swoDebugPrintf("PYRONET_TLS_CONNECT_STEP step=ssl_setup");
    ret = mbedtls_ssl_setup(&sec->ssl, &sec->conf);
    (void)swoDebugPrintf("PYRONET_TLS_CONNECT_RET step=ssl_setup ret=%d", ret);
    if (ret != 0) {
        tr_error("ssl setup fail");
        return -1;
    }

    // Defines MBEDTLS_SSL_CONF_RECV/SEND/RECV_TIMEOUT define global functions which should be the same for all
    // callers of mbedtls_ssl_set_bio_ctx and there should be only one ssl context. If these rules don't apply,
    // these defines can't be used.
#if !defined(MBEDTLS_SSL_CONF_RECV) && !defined(MBEDTLS_SSL_CONF_SEND) && !defined(MBEDTLS_SSL_CONF_RECV_TIMEOUT)
    // Set calbacks
    mbedtls_ssl_set_bio(&sec->ssl, sec, tls_sec_prot_lib_ssl_send, tls_sec_prot_lib_ssl_recv, NULL);
#else
    mbedtls_ssl_set_bio_ctx(&sec->ssl, sec);
#endif /* !defined(MBEDTLS_SSL_CONF_RECV) && !defined(MBEDTLS_SSL_CONF_SEND) && !defined(MBEDTLS_SSL_CONF_RECV_TIMEOUT) */

// Defines MBEDTLS_SSL_CONF_SET_TIMER/GET_TIMER define global functions which should be the same for all
// callers of mbedtls_ssl_set_timer_cb and there should be only one ssl context. If these rules don't apply,
// these defines can't be used.
#if !defined(MBEDTLS_SSL_CONF_SET_TIMER) && !defined(MBEDTLS_SSL_CONF_GET_TIMER)
    mbedtls_ssl_set_timer_cb(&sec->ssl, sec, tls_sec_prot_lib_ssl_set_timer, tls_sec_prot_lib_ssl_get_timer);
#endif /* !defined(MBEDTLS_SSL_CONF_SET_TIMER) && !defined(MBEDTLS_SSL_CONF_GET_TIMER) */

    // Configure certificates, keys and certificate revocation list
    (void)swoDebugPrintf("PYRONET_TLS_CONNECT_STEP step=certs");
    if (tls_sec_prot_lib_configure_certificates(sec, certs) != 0) {
        tr_error("cert conf fail");
        (void)swoDebugPrintf("PYRONET_TLS_CONNECT_FAIL reason=certs");
        return -1;
    }
    (void)swoDebugPrintf("PYRONET_TLS_CONNECT_RET step=certs ret=0");

#if !defined(MBEDTLS_SSL_CONF_SINGLE_CIPHERSUITE)
    // Configure ciphersuites
    static const int sec_suites[] = {
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8,
        0,
        0,
        0
    };
    mbedtls_ssl_conf_ciphersuites(&sec->conf, sec_suites);
#endif /* !defined(MBEDTLS_SSL_CONF_SINGLE_CIPHERSUITE) */

#ifdef TLS_SEC_PROT_LIB_TLS_DEBUG
    mbedtls_ssl_conf_dbg(&sec->conf, tls_sec_prot_lib_debug, sec);
    mbedtls_debug_set_threshold(5);
#endif

    // Export keys callback
    mbedtls_ssl_conf_export_keys_ext_cb(&sec->conf, tls_sec_prot_lib_ssl_export_keys, sec);

#if !defined(MBEDTLS_SSL_CONF_MIN_MINOR_VER) || !defined(MBEDTLS_SSL_CONF_MIN_MAJOR_VER)
    mbedtls_ssl_conf_min_version(&sec->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MAJOR_VERSION_3);
#endif /* !defined(MBEDTLS_SSL_CONF_MIN_MINOR_VER) || !defined(MBEDTLS_SSL_CONF_MIN_MAJOR_VER) */

#if !defined(MBEDTLS_SSL_CONF_MAX_MINOR_VER) || !defined(MBEDTLS_SSL_CONF_MAX_MAJOR_VER)
    mbedtls_ssl_conf_max_version(&sec->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MAJOR_VERSION_3);
#endif /* !defined(MBEDTLS_SSL_CONF_MAX_MINOR_VER) || !defined(MBEDTLS_SSL_CONF_MAX_MAJOR_VER) */

    // Set certificate verify callback
    mbedtls_ssl_set_verify(&sec->ssl, tls_sec_prot_lib_x509_crt_verify, sec);

    /* Currently assuming we are running fast enough HW that ECC calculations are not blocking any normal operation.
     *
     * If there is a problem with ECC calculations and those are taking too long in border router
     * MBEDTLS_ECP_RESTARTABLE feature needs to be enabled and public API is needed to allow it in border router
     * enabling should be done here.
     */
    (void)swoDebugPrintf("PYRONET_TLS_CONNECT_DONE");
    return 0;
}

#ifdef TLS_SEC_PROT_LIB_TLS_DEBUG
static void tls_sec_prot_lib_debug(void *ctx, int level, const char *file, int line, const char *string)
{
    (void) ctx;
    tr_debug("%i %s %i %s", level, file, line, string);
}
#endif

int8_t tls_sec_prot_lib_process(tls_security_t *sec)
{
    int32_t ret = -1;
    uint8_t steps = 0;

    while (ret != MBEDTLS_ERR_SSL_WANT_READ) {
        (void)swoDebugPrintf("PYRONET_TLS_HANDSHAKE_STEP_START step=%u state=%u",
                             (unsigned int)steps,
                             (unsigned int)sec->ssl.state);
        ret = mbedtls_ssl_handshake_step(&sec->ssl);
        (void)swoDebugPrintf("PYRONET_TLS_HANDSHAKE_STEP_RET step=%u ret=%ld state=%u",
                             (unsigned int)steps,
                             (long)ret,
                             (unsigned int)sec->ssl.state);
        steps++;

#if defined(MBEDTLS_ECP_RESTARTABLE) && defined(MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS)
        if (ret == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS /* || ret == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS */) {
            return TLS_SEC_PROT_LIB_CALCULATING;
        }
#endif

        if (ret && (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            tr_error("TLS error: %" PRId32, ret);
            return TLS_SEC_PROT_LIB_ERROR;
        }

        if (sec->ssl.state == MBEDTLS_SSL_HANDSHAKE_OVER) {
            return TLS_SEC_PROT_LIB_HANDSHAKE_OVER;
        }
    }

    return TLS_SEC_PROT_LIB_CONTINUE;
}

static void tls_sec_prot_lib_ssl_set_timer(void *ctx, uint32_t int_ms, uint32_t fin_ms)
{
    tls_security_t *sec = (tls_security_t *)ctx;
    sec->set_timer(sec->handle, int_ms, fin_ms);
}

static int tls_sec_prot_lib_ssl_get_timer(void *ctx)
{
    tls_security_t *sec = (tls_security_t *)ctx;
    return sec->get_timer(sec->handle);
}

static int tls_sec_prot_lib_ssl_send(void *ctx, const unsigned char *buf, size_t len)
{
    tls_security_t *sec = (tls_security_t *)ctx;
    return sec->send(sec->handle, buf, len);
}

static int tls_sec_prot_lib_ssl_recv(void *ctx, unsigned char *buf, size_t len)
{
    tls_security_t *sec = (tls_security_t *)ctx;
    int16_t ret = sec->receive(sec->handle, buf, len);

    if (ret == TLS_SEC_PROT_LIB_NO_DATA) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return ret;
}

static int tls_sec_prot_lib_ssl_export_keys(void *p_expkey, const unsigned char *ms,
                                            const unsigned char *kb, size_t maclen, size_t keylen,
                                            size_t ivlen, const unsigned char client_random[32],
                                            const unsigned char server_random[32],
                                            mbedtls_tls_prf_types tls_prf_type)
{
    (void) kb;
    (void) maclen;
    (void) keylen;
    (void) ivlen;

    tls_security_t *sec = (tls_security_t *)p_expkey;

    uint8_t eap_tls_key_material[128];
    uint8_t random[64];
    memcpy(random, client_random, 32);
    memcpy(&random[32], server_random, 32);

    int ret = mbedtls_ssl_tls_prf(tls_prf_type, ms, 48, "client EAP encryption",
                                  random, 64, eap_tls_key_material, 128);

    if (ret != 0) {
        tr_error("key material PRF error");
        return 0;
    }

    sec->export_keys(sec->handle, ms, eap_tls_key_material);
    return 0;
}

static int tls_sec_prot_lib_x509_crt_verify(void *ctx, mbedtls_x509_crt *crt, int certificate_depth, uint32_t *flags)
{
    tls_security_t *sec = (tls_security_t *) ctx;

    /* MD/PK forced by configuration flags and dynamic settings but traced also here
       to prevent invalid configurations/certificates */
    if (crt->sig_md != MBEDTLS_MD_SHA256) {
        tr_error("Invalid signature md algorithm");
    }
    if (crt->sig_pk != MBEDTLS_PK_ECDSA) {
        tr_error("Invalid signature pk algorithm");
    }
    if (*flags & MBEDTLS_X509_BADCERT_FUTURE) {
        tr_info("Certificate time future");
        *flags &= ~MBEDTLS_X509_BADCERT_FUTURE;
    }

    // Verify client/server certificate of the chain
    if (certificate_depth == 0) {
        return sec->crt_verify(sec, crt, flags);
    }

    // No further checks for intermediate and root certificates at the moment
    return 0;
}

static int8_t tls_sec_prot_lib_subject_alternative_name_validate(mbedtls_x509_crt *crt)
{
    mbedtls_asn1_sequence *seq = &crt->subject_alt_names;
    int8_t result = -1;
    while (seq) {
        mbedtls_x509_subject_alternative_name san;
        int ret_value = mbedtls_x509_parse_subject_alt_name((mbedtls_x509_buf *)&seq->buf, &san);
        if (ret_value == 0 && san.type == MBEDTLS_X509_SAN_OTHER_NAME) {
            // id-on-hardwareModuleName must be present (1.3.6.1.5.5.7.8.4)
            if (MBEDTLS_OID_CMP(MBEDTLS_OID_ON_HW_MODULE_NAME, &san.san.other_name.value.hardware_module_name.oid)) {
                // Traces hardwareModuleName (1.3.6.1.4.1.<enteprise number>.<model,version,etc.>)
                char buffer[30];
                ret_value = mbedtls_oid_get_numeric_string(buffer, sizeof(buffer), &san.san.other_name.value.hardware_module_name.oid);
                if (ret_value != MBEDTLS_ERR_OID_BUF_TOO_SMALL) {
                    tr_info("id-on-hardwareModuleName %s", buffer);
                }
                // Traces serial number as hex string
                mbedtls_x509_buf *val = &san.san.other_name.value.hardware_module_name.val;
                if (val->p) {
                    tr_info("id-on-hardwareModuleName hwSerialNum %s", trace_array(val->p, val->len));
                }
                result = 0;
            }
        } else {
            tr_debug("Ignored subject alt name: %i", san.type);
        }
        seq = seq->next;
    }
    return result;
}

static int8_t tls_sec_prot_lib_extended_key_usage_validate(mbedtls_x509_crt *crt)
{
#if defined(MBEDTLS_X509_CHECK_EXTENDED_KEY_USAGE)
    // Extended key usage must be present
    if (mbedtls_x509_crt_check_extended_key_usage(crt, MBEDTLS_OID_WISUN_FAN, sizeof(MBEDTLS_OID_WISUN_FAN) - 1) != 0) {
        tr_error("invalid extended key usage");
        return -1; // FAIL
    }
#endif
    return 0;
}

#ifdef HAVE_PAE_AUTH
static int tls_sec_prot_lib_x509_crt_idevid_ldevid_verify(tls_security_t *sec, mbedtls_x509_crt *crt, uint32_t *flags)
{
    // For both IDevID and LDevId both subject alternative name or extended key usage must be valid
    if (tls_sec_prot_lib_subject_alternative_name_validate(crt) < 0 ||
            tls_sec_prot_lib_extended_key_usage_validate(crt) < 0) {
        tr_info("no wisun fields on cert");
        if (sec->ext_cert_valid) {
            *flags |= MBEDTLS_X509_BADCERT_OTHER;
            return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
        }
    }
    return 0;
}
#endif

#ifdef HAVE_PAE_SUPP
static int tls_sec_prot_lib_x509_crt_server_verify(tls_security_t *sec, mbedtls_x509_crt *crt, uint32_t *flags)
{
    int8_t sane_res = tls_sec_prot_lib_subject_alternative_name_validate(crt);
    int8_t ext_key_res = tls_sec_prot_lib_extended_key_usage_validate(crt);

    // If either subject alternative name or extended key usage is present
    if (sane_res >= 0 || ext_key_res >= 0) {
        // Then both subject alternative name and extended key usage must be valid
        if (sane_res < 0 || ext_key_res < 0) {
            tr_info("no wisun fields on cert");
            if (sec->ext_cert_valid) {
                *flags |= MBEDTLS_X509_BADCERT_OTHER;
                return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
            }
        }
    }

    return 0;
}
#endif

static int tls_sec_lib_entropy_poll(void *ctx, unsigned char *output, size_t len, size_t *olen)
{
    (void)ctx;
    static unsigned long poll_count = 0;
    unsigned long poll_id = ++poll_count;
    size_t produce_len = (len > 32U) ? 32U : len;

    (void)swoDebugPrintf("PYRONET_TLS_ENTROPY_POLL_START id=%lu req_len=%u out_len=%u",
                         poll_id, (unsigned int)len, (unsigned int)produce_len);

    if ((output == NULL) || (olen == NULL)) {
        (void)swoDebugPrintf("PYRONET_TLS_ENTROPY_POLL_FAIL id=%lu reason=args",
                             poll_id);
        tr_error("entropy args fail");
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }

    for (uint16_t i = 0; i < produce_len;) {
        if ((i != 0U) && ((i % 32U) == 0U)) {
            (void)swoDebugPrintf("PYRONET_TLS_ENTROPY_POLL_PROGRESS id=%lu offset=%u",
                                 poll_id, (unsigned int)i);
        }

        uint32_t random_word = tls_sec_lib_debug_random_word();
        for (uint8_t byte = 0U; (byte < 4U) && (i < produce_len); byte++, i++) {
            output[i] = (unsigned char)(random_word & 0xffU);
            random_word >>= 8;
        }
    }
    *olen = produce_len;

    (void)swoDebugPrintf("PYRONET_TLS_ENTROPY_POLL_DONE id=%lu out_len=%u",
                         poll_id, (unsigned int)produce_len);
    return (0);
}

static int tls_sec_lib_seed_poll(void *ctx, unsigned char *output, size_t len)
{
    (void)ctx;
    static unsigned long seed_count = 0;
    unsigned long seed_id = ++seed_count;

    if (output == NULL) {
        (void)swoDebugPrintf("PYRONET_TLS_SEED_POLL_FAIL id=%lu reason=args", seed_id);
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }

    (void)swoDebugPrintf("PYRONET_TLS_SEED_POLL_START id=%lu len=%u",
                         seed_id, (unsigned int)len);

    for (size_t i = 0; i < len;) {
        (void)swoDebugPrintf("PYRONET_TLS_SEED_WORD_START id=%lu offset=%u",
                             seed_id, (unsigned int)i);
        uint32_t random_word = tls_sec_lib_debug_random_word();
        (void)swoDebugPrintf("PYRONET_TLS_SEED_WORD_GOT id=%lu offset=%u word=%08lx",
                             seed_id, (unsigned int)i, (unsigned long)random_word);
        for (uint8_t byte = 0U; (byte < 4U) && (i < len); byte++, i++) {
            output[i] = (unsigned char)(random_word & 0xffU);
            random_word >>= 8;
        }
        (void)swoDebugPrintf("PYRONET_TLS_SEED_WORD_DONE id=%lu offset=%u",
                             seed_id, (unsigned int)i);
    }

    (void)swoDebugPrintf("PYRONET_TLS_SEED_POLL_DONE id=%lu len=%u",
                         seed_id, (unsigned int)len);
    return 0;
}

static uint32_t tls_sec_lib_debug_random_word(void)
{
    static uint32_t state = 0U;

    if (state == 0U) {
        arm_random_module_init();
        state = arm_random_seed_get();
        if (state == 0U) {
            state = UINT32_C(0x9e3779b9);
        }
        (void)swoDebugWriteLine("PYRONET_TLS_ENTROPY_SEEDED");
    }

    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    return state;
}

#ifdef TLS_SEC_PROT_LIB_USE_MBEDTLS_PLATFORM_MEMORY
static void *tls_sec_prot_lib_mem_calloc(size_t count, size_t size)
{
    void *mem_ptr = ns_dyn_mem_temporary_alloc(count * size);

    if (mem_ptr) {
        // Calloc should initialize with zero
        memset(mem_ptr, 0, count * size);
    }
    return mem_ptr;
}

static void tls_sec_prot_lib_mem_free(void *ptr)
{
    ns_dyn_mem_free(ptr);
}
#endif

#else /* WS_MBEDTLS_SECURITY_ENABLED */
#error "*** TI WISUN FAN CODE MUST NOT COME HERE. CHECK MBED TLS CONFIGURATION ***"
int8_t tls_sec_prot_lib_connect(tls_security_t *sec, bool is_server, const sec_prot_certs_t *certs)
{
    (void)sec;
    (void)is_server;
    (void)certs;
    return 0;
}

void tls_sec_prot_lib_free(tls_security_t *sec)
{
    (void)sec;
}

int8_t tls_sec_prot_lib_init(tls_security_t *sec)
{
    (void)sec;
    return 0;
}

int8_t tls_sec_prot_lib_process(tls_security_t *sec)
{
    (void)sec;
    return 0;
}

void tls_sec_prot_lib_set_cb_register(tls_security_t *sec, void *handle,
                                      tls_sec_prot_lib_send *send, tls_sec_prot_lib_receive *receive,
                                      tls_sec_prot_lib_export_keys *export_keys, tls_sec_prot_lib_set_timer *set_timer,
                                      tls_sec_prot_lib_get_timer *get_timer)
{
    (void)sec;
    (void)handle;
    (void)send;
    (void)receive;
    (void)export_keys;
    (void)set_timer;
    (void)get_timer;
}

uint16_t tls_sec_prot_lib_size(void)
{
    return 0;
}
#endif /* WS_MBEDTLS_SECURITY_ENABLED */
#endif /* HAVE_WS */
