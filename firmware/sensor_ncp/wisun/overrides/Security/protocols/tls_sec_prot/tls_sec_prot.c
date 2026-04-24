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
#include <string.h>
#include "ns_types.h"
#include "ns_list.h"
#include "ns_trace.h"
#include "nsdynmemLIB.h"
#include "fhss_config.h"
#include "Service_Libs/Trickle/trickle.h"
#include "NWK_INTERFACE/Include/protocol.h"
#include "6LoWPAN/ws/ws_config.h"
#include "Security/protocols/sec_prot_cfg.h"
#include "Security/kmp/kmp_addr.h"
#include "Security/kmp/kmp_api.h"
#include "Security/PANA/pana_eap_header.h"
#include "Security/eapol/eapol_helper.h"
#include "Security/protocols/sec_prot_certs.h"
#include "Security/protocols/sec_prot_keys.h"
#include "Security/protocols/sec_prot.h"
#include "Security/protocols/sec_prot_lib.h"
#include "Security/protocols/eap_tls_sec_prot/eap_tls_sec_prot_lib.h"
#include "Security/protocols/tls_sec_prot/tls_sec_prot.h"
#include "Security/protocols/tls_sec_prot/tls_sec_prot_lib.h"
#include "pyronet_wisun_diag.h"
#include "swo_debug.h"

#ifdef HAVE_WS

#define TRACE_GROUP "tlsp"

typedef enum {
    TLS_STATE_INIT = SEC_STATE_INIT,
    TLS_STATE_CREATE_REQ = SEC_STATE_CREATE_REQ,
    TLS_STATE_CREATE_RESP = SEC_STATE_CREATE_RESP,
    TLS_STATE_CREATE_IND = SEC_STATE_CREATE_IND,

    TLS_STATE_CLIENT_HELLO = SEC_STATE_FIRST,
    TLS_STATE_CONFIGURE,
    TLS_STATE_PROCESS,

    TLS_STATE_FINISH = SEC_STATE_FINISH,
    TLS_STATE_FINISHED = SEC_STATE_FINISHED
} eap_tls_sec_prot_state_e;

typedef struct tls_sec_prot_lib_int_s tls_sec_prot_lib_int_t;

typedef struct {
    sec_prot_common_t             common;            /**< Common data */
    uint8_t                       new_pmk[PMK_LEN];  /**< New Pair Wise Master Key */
    tls_data_t                    tls_send;          /**< TLS send buffer */
    tls_data_t                    tls_recv;          /**< TLS receive buffer */
    uint32_t                      int_timer;         /**< TLS intermediate timer timeout */
    uint32_t                      fin_timer;         /**< TLS final timer timeout */
    bool                          fin_timer_timeout; /**< TLS final timer has timeouted */
    bool                          timer_running : 1; /**< TLS timer running */
    bool                          finished : 1;      /**< TLS finished */
    bool                          calculating : 1;   /**< TLS is calculating */
#ifdef SERVER_TLS_EC_CALC_QUEUE
    bool                          queued : 1;        /**< TLS is queued */
#endif
    bool                          library_init : 1;  /**< TLS library has been initialized */
    tls_sec_prot_lib_int_t        *tls_sec_inst;     /**< TLS security library storage, SHALL BE THE LAST FIELD */
} tls_sec_prot_int_t;

// TLS server EC queue is currently disabled, since EC calculation is made on server in one go
#ifdef SERVER_TLS_EC_CALC_QUEUE
typedef struct {
    ns_list_link_t link;                             /**< Link */
    sec_prot_t *prot;                                /**< Protocol instance */
} tls_sec_prot_queue_t;
#endif

static uint16_t tls_sec_prot_size(void);
static int8_t client_tls_sec_prot_init(sec_prot_t *prot);
static int8_t server_tls_sec_prot_init(sec_prot_t *prot);

static void tls_sec_prot_create_request(sec_prot_t *prot, sec_prot_keys_t *sec_keys);
static void tls_sec_prot_create_response(sec_prot_t *prot, sec_prot_result_e result);
static void tls_sec_prot_delete(sec_prot_t *prot);
static int8_t tls_sec_prot_receive(sec_prot_t *prot, void *pdu, uint16_t size);
static void tls_sec_prot_finished_send(sec_prot_t *prot);

static void client_tls_sec_prot_state_machine(sec_prot_t *prot);
static void server_tls_sec_prot_state_machine(sec_prot_t *prot);

static void tls_sec_prot_timer_timeout(sec_prot_t *prot, uint16_t ticks);

static int16_t tls_sec_prot_tls_send(void *handle, const void *buf, size_t len);
static int16_t tls_sec_prot_tls_receive(void *handle, unsigned char *buf, size_t len);
static void tls_sec_prot_tls_export_keys(void *handle, const uint8_t *master_secret, const uint8_t *eap_tls_key_material);
static void tls_sec_prot_tls_set_timer(void *handle, uint32_t inter, uint32_t fin);
static int8_t tls_sec_prot_tls_get_timer(void *handle);

static int8_t tls_sec_prot_tls_configure_and_connect(sec_prot_t *prot, bool is_server);

#ifdef SERVER_TLS_EC_CALC_QUEUE
static bool tls_sec_prot_queue_check(sec_prot_t *prot);
static bool tls_sec_prot_queue_process(sec_prot_t *prot);
static void tls_sec_prot_queue_remove(sec_prot_t *prot);
#else
#define tls_sec_prot_queue_process(prot) true
#define tls_sec_prot_queue_remove(prot)
#endif /* SERVER_TLS_EC_CALC_QUEUE */

static uint16_t tls_sec_prot_send_buffer_size_get(sec_prot_t *prot);
static const char *tls_sec_prot_state_trace(uint8_t state);
static const char *tls_sec_prot_lib_result_trace(int8_t result);

#define tls_sec_prot_get(prot) (tls_sec_prot_int_t *) &prot->data

#ifdef SERVER_TLS_EC_CALC_QUEUE
static NS_LIST_DEFINE(tls_sec_prot_queue, tls_sec_prot_queue_t, link);
#endif

static const char *tls_sec_prot_state_trace(uint8_t state)
{
    switch (state) {
        case TLS_STATE_INIT:
            return "INIT";
        case TLS_STATE_CREATE_REQ:
            return "CREATE_REQ";
        case TLS_STATE_CREATE_RESP:
            return "CREATE_RESP";
        case TLS_STATE_CREATE_IND:
            return "CREATE_IND";
        case TLS_STATE_CLIENT_HELLO:
            return "CLIENT_HELLO";
        case TLS_STATE_CONFIGURE:
            return "CONFIGURE";
        case TLS_STATE_PROCESS:
            return "PROCESS";
        case TLS_STATE_FINISH:
            return "FINISH";
        case TLS_STATE_FINISHED:
            return "FINISHED";
        default:
            return "UNKNOWN";
    }
}

static const char *tls_sec_prot_lib_result_trace(int8_t result)
{
    switch (result) {
        case TLS_SEC_PROT_LIB_ERROR:
            return "ERROR";
        case TLS_SEC_PROT_LIB_NO_DATA:
            return "NO_DATA";
        case TLS_SEC_PROT_LIB_CONTINUE:
            return "CONTINUE";
        case TLS_SEC_PROT_LIB_CALCULATING:
            return "CALCULATING";
        case TLS_SEC_PROT_LIB_HANDSHAKE_OVER:
            return "HANDSHAKE_OVER";
        default:
            return "UNKNOWN";
    }
}

int8_t client_tls_sec_prot_register(kmp_service_t *service)
{
    if (!service) {
        return -1;
    }

    if (kmp_service_sec_protocol_register(service, TLS_PROT, tls_sec_prot_size, client_tls_sec_prot_init) < 0) {
        return -1;
    }

    return 0;
}

int8_t server_tls_sec_prot_register(kmp_service_t *service)
{
    if (!service) {
        return -1;
    }

    if (kmp_service_sec_protocol_register(service, TLS_PROT, tls_sec_prot_size, server_tls_sec_prot_init) < 0) {
        return -1;
    }

    return 0;
}

static uint16_t tls_sec_prot_size(void)
{
    return sizeof(tls_sec_prot_int_t) + tls_sec_prot_lib_size();
}

static int8_t client_tls_sec_prot_init(sec_prot_t *prot)
{
    prot->create_req = tls_sec_prot_create_request;
    prot->create_resp = NULL;
    prot->receive = tls_sec_prot_receive;
    prot->delete = tls_sec_prot_delete;
    prot->state_machine = client_tls_sec_prot_state_machine;
    prot->timer_timeout = tls_sec_prot_timer_timeout;
    prot->finished_send = tls_sec_prot_finished_send;

    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    sec_prot_init(&data->common);
    sec_prot_state_set(prot, &data->common, TLS_STATE_INIT);

    memset(data->new_pmk, 0, PMK_LEN);
    data->finished = false;
    // Set from security parameters
    eap_tls_sec_prot_lib_message_init(&data->tls_recv);
    eap_tls_sec_prot_lib_message_init(&data->tls_send);
    data->int_timer = 0;
    data->fin_timer = 0;
    data->fin_timer_timeout = false;
    data->timer_running = false;
    data->calculating = false;
#ifdef SERVER_TLS_EC_CALC_QUEUE
    data->queued = false;
#endif
    data->library_init = false;
    return 0;
}

static int8_t server_tls_sec_prot_init(sec_prot_t *prot)
{
    prot->create_req = NULL;
    prot->create_resp = tls_sec_prot_create_response;
    prot->receive = tls_sec_prot_receive;
    prot->delete = tls_sec_prot_delete;
    prot->state_machine = server_tls_sec_prot_state_machine;
    prot->timer_timeout = tls_sec_prot_timer_timeout;
    prot->finished_send = tls_sec_prot_finished_send;

    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    sec_prot_init(&data->common);
    sec_prot_state_set(prot, &data->common, TLS_STATE_INIT);

    memset(data->new_pmk, 0, PMK_LEN);
    data->finished = false;
    // Set from security parameters
    eap_tls_sec_prot_lib_message_init(&data->tls_recv);
    eap_tls_sec_prot_lib_message_init(&data->tls_send);
    data->int_timer = 0;
    data->fin_timer = 0;
    data->fin_timer_timeout = false;
    data->timer_running = false;
    data->calculating = false;
#ifdef SERVER_TLS_EC_CALC_QUEUE
    data->queued = false;
#endif
    data->library_init = false;
    return 0;
}

static void tls_sec_prot_delete(sec_prot_t *prot)
{
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);
    eap_tls_sec_prot_lib_message_free(&data->tls_send);
    eap_tls_sec_prot_lib_message_free(&data->tls_recv);
    if (data->library_init) {
        tr_info("TLS: free library");
        tls_sec_prot_lib_free((tls_security_t *) &data->tls_sec_inst);
    }
    tls_sec_prot_queue_remove(prot);
}

static void tls_sec_prot_create_request(sec_prot_t *prot, sec_prot_keys_t *sec_keys)
{
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    prot->sec_keys = sec_keys;

    (void)swoDebugPrintf("PYRONET_TLS_CREATE_REQ sec_keys=%p certs=%p state=%s",
                         (void *)sec_keys,
                         sec_keys ? (void *)sec_keys->certs : NULL,
                         tls_sec_prot_state_trace(sec_prot_state_get(&data->common)));

    // Call state machine
    prot->state_machine_call(prot);
}

static void tls_sec_prot_create_response(sec_prot_t *prot, sec_prot_result_e result)
{
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    // Call state machine
    sec_prot_result_set(&data->common, result);
    prot->state_machine_call(prot);
}

static int8_t tls_sec_prot_receive(sec_prot_t *prot, void *pdu, uint16_t size)
{
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    // Discards old data
    eap_tls_sec_prot_lib_message_free(&data->tls_recv);

    data->tls_recv.data = pdu;
    data->tls_recv.total_len = size;

    prot->state_machine(prot);

    return 0;
}

static void tls_sec_prot_finished_send(sec_prot_t *prot)
{
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);
    prot->timer_start(prot);
    sec_prot_state_set(prot, &data->common, TLS_STATE_FINISHED);
}

static void tls_sec_prot_timer_timeout(sec_prot_t *prot, uint16_t ticks)
{
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    if (data->timer_running) {
        if (data->int_timer > ticks) {
            data->int_timer -= ticks;
        } else {
            data->int_timer = 0;
        }

        if (data->fin_timer > ticks) {
            data->fin_timer -= ticks;
        } else {
            if (data->fin_timer > 0) {
                data->fin_timer_timeout = true;
                data->fin_timer = 0;
            }
        }
    }

    /* Checks if TLS sessions queue is enabled, and if queue is enabled whether the
       session is first in the queue i.e. allowed to process */
    if (tls_sec_prot_queue_process(prot)) {
        if (data->fin_timer_timeout) {
            data->fin_timer_timeout = false;
            prot->state_machine(prot);
        } else if (data->calculating
#ifdef SERVER_TLS_EC_CALC_QUEUE
                   || data->queued
#endif
                  ) {
            prot->state_machine(prot);
        }
    }

    sec_prot_timer_timeout_handle(prot, &data->common, NULL, ticks);
}

static void client_tls_sec_prot_state_machine(sec_prot_t *prot)
{
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);
    int8_t result;

    switch (sec_prot_state_get(&data->common)) {
        case TLS_STATE_INIT:
            tr_debug("TLS: init");
            (void)swoDebugPrintf("PYRONET_TLS_STATE state=INIT action=start_timer");
            sec_prot_state_set(prot, &data->common, TLS_STATE_CREATE_REQ);
            prot->timer_start(prot);
            // Set default timeout for the total maximum length of the negotiation
            sec_prot_default_timeout_set(&data->common);
            break;

        // Wait KMP-CREATE.request
        case TLS_STATE_CREATE_REQ:
            tr_debug("TLS: start");
            (void)swoDebugPrintf("PYRONET_TLS_STATE state=CREATE_REQ action=create_conf_ok");

            prot->create_conf(prot, SEC_RESULT_OK);

            sec_prot_state_set(prot, &data->common, TLS_STATE_CONFIGURE);

            prot->state_machine_call(prot);
            break;

        case TLS_STATE_CONFIGURE:
            (void)swoDebugPrintf("PYRONET_TLS_CONFIGURE_START role=client sec_keys=%p certs=%p",
                                 (void *)prot->sec_keys,
                                 prot->sec_keys ? (void *)prot->sec_keys->certs : NULL);
            pyronet_wisun_diag_record(PYRONET_WISUN_DIAG_TLS_CONFIGURE_START,
                                      prot->sec_keys != NULL,
                                      (prot->sec_keys && prot->sec_keys->certs) ? 1 : 0,
                                      0);
            if (tls_sec_prot_tls_configure_and_connect(prot, false) < 0) {
                (void)swoDebugPrintf("PYRONET_TLS_CONFIGURE_FAIL role=client");
                sec_prot_result_set(&data->common, SEC_RESULT_CONF_ERROR);
                sec_prot_state_set(prot, &data->common, TLS_STATE_FINISH);
                return;
            }
            (void)swoDebugPrintf("PYRONET_TLS_CONFIGURE_OK role=client");
            sec_prot_state_set(prot, &data->common, TLS_STATE_PROCESS);
            prot->state_machine(prot);
            break;

        case TLS_STATE_PROCESS:
            (void)swoDebugPrintf("PYRONET_TLS_PROCESS_START recv_total=%u recv_handled=%u send_pending=%u calculating=%u",
                                 (unsigned int)data->tls_recv.total_len,
                                 (unsigned int)data->tls_recv.handled_len,
                                 (unsigned int)(data->tls_send.data != NULL),
                                 (unsigned int)data->calculating);
            result = tls_sec_prot_lib_process((tls_security_t *) &data->tls_sec_inst);
            (void)swoDebugPrintf("PYRONET_TLS_PROCESS_RESULT result=%s(%d) send_data=%u send_len=%u handled_len=%u",
                                 tls_sec_prot_lib_result_trace(result),
                                 (int)result,
                                 (unsigned int)(data->tls_send.data != NULL),
                                 (unsigned int)data->tls_send.total_len,
                                 (unsigned int)data->tls_send.handled_len);
            pyronet_wisun_diag_record(PYRONET_WISUN_DIAG_TLS_PROCESS_RESULT,
                                      result,
                                      data->tls_send.data != NULL,
                                      data->tls_send.handled_len);

            if (result == TLS_SEC_PROT_LIB_CALCULATING) {
                data->calculating = true;
                prot->state_machine_call(prot);
                return;
            } else {
                data->calculating = false;
            }

            if (data->tls_send.data) {
                int8_t send_result = prot->send(prot, data->tls_send.data, data->tls_send.handled_len);
                (void)swoDebugPrintf("PYRONET_TLS_SEND_TO_EAP result=%d len=%u",
                                     (int)send_result,
                                     (unsigned int)data->tls_send.handled_len);
                pyronet_wisun_diag_record(PYRONET_WISUN_DIAG_TLS_SEND_TO_EAP,
                                          send_result,
                                          data->tls_send.handled_len,
                                          0);
                eap_tls_sec_prot_lib_message_init(&data->tls_send);
            }

            if (result != TLS_SEC_PROT_LIB_CONTINUE) {
                if (result == TLS_SEC_PROT_LIB_ERROR) {
                    tr_error("TLS: error");
                    (void)swoDebugPrintf("PYRONET_TLS_PROCESS_ERROR action=finish_error");
                    sec_prot_result_set(&data->common, SEC_RESULT_ERROR);
                }
                sec_prot_state_set(prot, &data->common, TLS_STATE_FINISH);
            }
            break;

        case TLS_STATE_FINISH:
            tr_debug("TLS: finish");
            (void)swoDebugPrintf("PYRONET_TLS_FINISH result=%d library_init=%u finished=%u",
                                 (int)sec_prot_result_get(&data->common),
                                 (unsigned int)data->library_init,
                                 (unsigned int)data->finished);

            data->calculating = false;

            if (sec_prot_result_ok_check(&data->common)) {
                sec_prot_keys_pmk_write(prot->sec_keys, data->new_pmk, prot->sec_cfg->timer_cfg.pmk_lifetime);
            }

            // KMP-FINISHED.indication,
            prot->finished_ind(prot, sec_prot_result_get(&data->common), prot->sec_keys);
            sec_prot_state_set(prot, &data->common, TLS_STATE_FINISHED);

            tls_sec_prot_lib_free((tls_security_t *) &data->tls_sec_inst);
            data->library_init = false;
            break;

        case TLS_STATE_FINISHED:
            tr_debug("TLS: finished, free %s", data->library_init ? "T" : "F");
            (void)swoDebugPrintf("PYRONET_TLS_FINISHED library_init=%u",
                                 (unsigned int)data->library_init);
            if (data->library_init) {
                tls_sec_prot_lib_free((tls_security_t *) &data->tls_sec_inst);
                data->library_init = false;
            }
            prot->timer_stop(prot);
            prot->finished(prot);
            break;

        default:
            break;
    }
}

static void server_tls_sec_prot_state_machine(sec_prot_t *prot)
{
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);
    int8_t result;
#ifdef SERVER_TLS_EC_CALC_QUEUE
    bool client_hello = false;
#endif

    switch (sec_prot_state_get(&data->common)) {
        case TLS_STATE_INIT:
            tr_debug("TLS: init");
            sec_prot_state_set(prot, &data->common, TLS_STATE_CLIENT_HELLO);
            prot->timer_start(prot);
            // Set default timeout for the total maximum length of the negotiation
            sec_prot_default_timeout_set(&data->common);
            break;

        // Wait EAP request, Identity (starts handshake on supplicant)
        case TLS_STATE_CLIENT_HELLO:
            tr_debug("TLS: start, eui-64: %s", trace_array(sec_prot_remote_eui_64_addr_get(prot), 8));

#ifdef SERVER_TLS_EC_CALC_QUEUE
            client_hello = true;
#endif

            sec_prot_state_set(prot, &data->common, TLS_STATE_CREATE_RESP);

            // Send KMP-CREATE.indication
            prot->create_ind(prot);
            break;

        // Wait KMP-CREATE.response
        case TLS_STATE_CREATE_RESP:
            if (sec_prot_result_ok_check(&data->common)) {
                sec_prot_state_set(prot, &data->common, TLS_STATE_CONFIGURE);
                prot->state_machine_call(prot);
            } else {
                // Ready to be deleted
                sec_prot_state_set(prot, &data->common, TLS_STATE_FINISHED);
            }
            break;

        case TLS_STATE_CONFIGURE:
            if (tls_sec_prot_tls_configure_and_connect(prot, true) < 0) {
                sec_prot_result_set(&data->common, SEC_RESULT_CONF_ERROR);
                sec_prot_state_set(prot, &data->common, TLS_STATE_FINISH);
                return;
            }
            sec_prot_state_set(prot, &data->common, TLS_STATE_PROCESS);
            prot->state_machine(prot);
            break;

        case TLS_STATE_PROCESS:
#ifdef SERVER_TLS_EC_CALC_QUEUE
            // If not client hello, reserves slot on TLS queue
            if (!client_hello && !tls_sec_prot_queue_check(prot)) {
                data->queued = true;
                return;
            } else {
                data->queued = false;
            }
#endif

            result = tls_sec_prot_lib_process((tls_security_t *) &data->tls_sec_inst);

            if (result == TLS_SEC_PROT_LIB_CALCULATING) {
                data->calculating = true;
                prot->state_machine_call(prot);
                return;
            } else {
                data->calculating = false;
            }

            if (data->tls_send.data) {
                prot->send(prot, data->tls_send.data, data->tls_send.handled_len);
                eap_tls_sec_prot_lib_message_init(&data->tls_send);
            }

            if (result != TLS_SEC_PROT_LIB_CONTINUE) {
                if (result == TLS_SEC_PROT_LIB_ERROR) {
                    tr_error("TLS: error, eui-64: %s", trace_array(sec_prot_remote_eui_64_addr_get(prot), 8));
                    sec_prot_result_set(&data->common, SEC_RESULT_ERROR);
                }
                sec_prot_state_set(prot, &data->common, TLS_STATE_FINISH);
            }
            break;

        case TLS_STATE_FINISH:
            tr_debug("TLS: finish, eui-64: %s", trace_array(sec_prot_remote_eui_64_addr_get(prot), 8));

            data->calculating = false;

            if (sec_prot_result_ok_check(&data->common)) {
                sec_prot_keys_pmk_write(prot->sec_keys, data->new_pmk, prot->sec_cfg->timer_cfg.pmk_lifetime);
            }

            // KMP-FINISHED.indication,
            prot->finished_ind(prot, sec_prot_result_get(&data->common), prot->sec_keys);
            sec_prot_state_set(prot, &data->common, TLS_STATE_FINISHED);

            tls_sec_prot_queue_remove(prot);
            tls_sec_prot_lib_free((tls_security_t *) &data->tls_sec_inst);
            data->library_init = false;
            break;

        case TLS_STATE_FINISHED: {
            uint8_t *remote_eui_64 = sec_prot_remote_eui_64_addr_get(prot);
            tr_debug("TLS: finished, eui-64: %s free %s", remote_eui_64 ? trace_array(sec_prot_remote_eui_64_addr_get(prot), 8) : "not set", data->library_init ? "T" : "F");
            if (data->library_init) {
                tls_sec_prot_lib_free((tls_security_t *) &data->tls_sec_inst);
                data->library_init = false;
            }
            prot->timer_stop(prot);
            prot->finished(prot);
            break;
        }
        default:
            break;
    }
}

static int16_t tls_sec_prot_tls_send(void *handle, const void *buf, size_t len)
{
    sec_prot_t *prot = handle;
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    if (!data->tls_send.data) {
        uint16_t buffer_len = tls_sec_prot_send_buffer_size_get(prot);
        eap_tls_sec_prot_lib_message_allocate(&data->tls_send, prot->header_size, buffer_len);
        (void)swoDebugPrintf("PYRONET_TLS_SEND_ALLOC header=%u buffer_len=%u data=%p",
                             (unsigned int)prot->header_size,
                             (unsigned int)buffer_len,
                             (void *)data->tls_send.data);
    }
    if (!data->tls_send.data) {
        (void)swoDebugPrintf("PYRONET_TLS_SEND_FAIL reason=alloc len=%u",
                             (unsigned int)len);
        return -1;
    }

    /* If send buffer is too small for the TLS payload, re-allocates */
    uint16_t new_len = prot->header_size + data->tls_send.handled_len + len;
    if (new_len > data->tls_send.total_len) {
        tr_error("TLS send buffer size too small: %i < %i, allocating new: %i", data->tls_send.total_len, new_len, data->tls_send.total_len + TLS_SEC_PROT_SEND_BUFFER_SIZE_INCREMENT);
        if (eap_tls_sec_prot_lib_message_realloc(&data->tls_send, prot->header_size,
                                                 data->tls_send.total_len + TLS_SEC_PROT_SEND_BUFFER_SIZE_INCREMENT) < 0) {
            (void)swoDebugPrintf("PYRONET_TLS_SEND_FAIL reason=realloc old_total=%u needed=%u",
                                 (unsigned int)data->tls_send.total_len,
                                 (unsigned int)new_len);
            return -1;
        }
        (void)swoDebugPrintf("PYRONET_TLS_SEND_REALLOC new_total=%u needed=%u",
                             (unsigned int)data->tls_send.total_len,
                             (unsigned int)new_len);
    }

    memcpy(data->tls_send.data + prot->header_size + data->tls_send.handled_len, buf, len);
    data->tls_send.handled_len += len;

    (void)swoDebugPrintf("PYRONET_TLS_SEND_BUFFERED len=%u handled=%u total=%u",
                         (unsigned int)len,
                         (unsigned int)data->tls_send.handled_len,
                         (unsigned int)data->tls_send.total_len);

    return len;
}

static int16_t tls_sec_prot_tls_receive(void *handle, unsigned char *buf, size_t len)
{
    sec_prot_t *prot = handle;
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    if (data->tls_recv.data && len > 0) {

        uint16_t copy_len = len;
        bool all_copied = false;

        if ((uint16_t) copy_len >= data->tls_recv.total_len - data->tls_recv.handled_len) {
            copy_len = data->tls_recv.total_len - data->tls_recv.handled_len;
            all_copied = true;
        }

        memcpy(buf, data->tls_recv.data + data->tls_recv.handled_len, copy_len);

        data->tls_recv.handled_len += copy_len;

        if (all_copied) {
            eap_tls_sec_prot_lib_message_free(&data->tls_recv);
        }

        return copy_len;
    }

    return TLS_SEC_PROT_LIB_NO_DATA;
}

static void tls_sec_prot_tls_export_keys(void *handle, const uint8_t *master_secret, const uint8_t *eap_tls_key_material)
{
    (void) master_secret;

    sec_prot_t *prot = handle;
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    if (eap_tls_key_material) {
        memcpy(data->new_pmk, eap_tls_key_material, PMK_LEN);
    }

#ifdef EXTRA_DEBUG_INFO
    const uint8_t *print_data = eap_tls_key_material;
    uint16_t print_data_len = 128;
    while (true) {
        tr_debug("EAP-TLS key material %s\n", trace_array(print_data, print_data_len > 32 ? 32 : print_data_len));
        if (print_data_len > 32) {
            print_data_len -= 32;
            print_data += 32;
        } else {
            break;
        }
    }
#endif
}

static void tls_sec_prot_tls_set_timer(void *handle, uint32_t inter, uint32_t fin)
{
    sec_prot_t *prot = handle;
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    if (fin == 0) {
        data->timer_running = false;
        data->int_timer = 0;
        data->fin_timer = 0;
        return;
    }

    data->timer_running = true;
    data->int_timer = inter / 100;
    data->fin_timer = fin / 100;
}

static int8_t tls_sec_prot_tls_get_timer(void *handle)
{
    sec_prot_t *prot = handle;
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    if (!data->timer_running) {
        return TLS_SEC_PROT_LIB_TIMER_CANCELLED;
    } else if (data->fin_timer == 0) {
        return TLS_SEC_PROT_LIB_TIMER_FIN_EXPIRY;
    } else if (data->int_timer == 0) {
        return TLS_SEC_PROT_LIB_TIMER_INT_EXPIRY;
    }

    return TLS_SEC_PROT_LIB_TIMER_NO_EXPIRY;
}

static int8_t tls_sec_prot_tls_configure_and_connect(sec_prot_t *prot, bool is_server)
{
    tls_sec_prot_int_t *data = tls_sec_prot_get(prot);

    (void)swoDebugPrintf("PYRONET_TLS_CONFIG_ENTER role=%s prot=%p sec_keys=%p certs=%p",
                         is_server ? "server" : "client",
                         (void *)prot,
                         (void *)prot->sec_keys,
                         prot->sec_keys ? (void *)prot->sec_keys->certs : NULL);

    // Must be free if library initialize is done
    data->library_init = true;
    (void)swoDebugPrintf("PYRONET_TLS_LIB_INIT_START role=%s sec_keys=%p certs=%p",
                         is_server ? "server" : "client",
                         (void *)prot->sec_keys,
                         prot->sec_keys ? (void *)prot->sec_keys->certs : NULL);
    if (tls_sec_prot_lib_init((tls_security_t *)&data->tls_sec_inst) < 0) {
        tr_error("TLS: library init fail");
        (void)swoDebugPrintf("PYRONET_TLS_LIB_INIT_FAIL role=%s",
                             is_server ? "server" : "client");
        return -1;
    }
    (void)swoDebugPrintf("PYRONET_TLS_LIB_INIT_OK role=%s inst=%p",
                         is_server ? "server" : "client",
                         (void *)data->tls_sec_inst);

    tls_sec_prot_lib_set_cb_register((tls_security_t *)&data->tls_sec_inst, prot,
                                     tls_sec_prot_tls_send, tls_sec_prot_tls_receive, tls_sec_prot_tls_export_keys,
                                     tls_sec_prot_tls_set_timer, tls_sec_prot_tls_get_timer);

    if (tls_sec_prot_lib_connect((tls_security_t *)&data->tls_sec_inst, is_server, prot->sec_keys->certs) < 0) {
        tr_error("TLS: library connect fail");
        (void)swoDebugPrintf("PYRONET_TLS_LIB_CONNECT_FAIL role=%s certs=%p",
                             is_server ? "server" : "client",
                             prot->sec_keys ? (void *)prot->sec_keys->certs : NULL);
        pyronet_wisun_diag_record(PYRONET_WISUN_DIAG_TLS_LIB_CONNECT_FAIL,
                                  is_server,
                                  prot->sec_keys != NULL,
                                  (prot->sec_keys && prot->sec_keys->certs) ? 1 : 0);
        return -1;
    }
    (void)swoDebugPrintf("PYRONET_TLS_LIB_CONNECT_OK role=%s",
                         is_server ? "server" : "client");

    return 0;
}

#ifdef SERVER_TLS_EC_CALC_QUEUE
static bool tls_sec_prot_queue_check(sec_prot_t *prot)
{
    bool queue_add = true;
    bool queue_continue = false;
    uint8_t entry_index = 0;

    // Checks if TLS queue is empty or this instance is the first entry
    if (ns_list_is_empty(&tls_sec_prot_queue)) {
        queue_continue = true;
    } else {
        ns_list_foreach(tls_sec_prot_queue_t, entry, &tls_sec_prot_queue) {
            if (entry->prot == prot) {
                queue_add = false;
                if (entry_index < 3) {
                    queue_continue = true;
                    break;
                } else {
                    queue_continue = false;
                }
            }
            entry_index++;
        }
    }

    // Adds entry to queue if not there already
    if (queue_add) {
        tr_debug("TLS QUEUE add index: %i, eui-64: %s", entry_index, trace_array(sec_prot_remote_eui_64_addr_get(prot), 8));
        tls_sec_prot_queue_t *entry = ns_dyn_mem_temporary_alloc(sizeof(tls_sec_prot_queue_t));
        if (entry) {
            entry->prot = prot;
            ns_list_add_to_end(&tls_sec_prot_queue, entry);
        }
    }

    return queue_continue;
}

static bool tls_sec_prot_queue_process(sec_prot_t *prot)
{
    if (ns_list_is_empty(&tls_sec_prot_queue)) {
        return true;
    }

    uint8_t entry_index = 0;
    ns_list_foreach(tls_sec_prot_queue_t, entry, &tls_sec_prot_queue) {
        if (entry->prot == prot) {
            return true;
        }
        if (entry_index > 2) {
            return false;
        }
        entry_index++;
    }

    return false;
}

static void tls_sec_prot_queue_remove(sec_prot_t *prot)
{
    ns_list_foreach_safe(tls_sec_prot_queue_t, entry, &tls_sec_prot_queue) {
        if (entry->prot == prot) {
            ns_list_remove(&tls_sec_prot_queue, entry);
            ns_dyn_mem_free(entry);
            tr_debug("TLS QUEUE remove%s, eui-64: %s", ns_list_is_empty(&tls_sec_prot_queue) ? " last" : "", trace_array(sec_prot_remote_eui_64_addr_get(prot), 8));
        }
    }
}
#endif

static uint16_t tls_sec_prot_send_buffer_size_get(sec_prot_t *prot)
{
    return TLS_SEC_PROT_SEND_BUFFER_SIZE + sec_prot_certs_own_cert_chain_len_get(prot->sec_keys->certs);
}

#endif /* HAVE_WS */
