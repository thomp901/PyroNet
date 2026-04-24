#ifndef PYRONET_WISUN_DIAG_H
#define PYRONET_WISUN_DIAG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum pyronet_wisun_diag_event
{
    PYRONET_WISUN_DIAG_EAP_RX = 1,
    PYRONET_WISUN_DIAG_EAP_IDENTITY_TX = 2,
    PYRONET_WISUN_DIAG_EAP_TLS_START_RX = 3,
    PYRONET_WISUN_DIAG_EAP_TLS_INIT_FAIL = 4,
    PYRONET_WISUN_DIAG_EAP_TLS_CREATE_REQ = 5,
    PYRONET_WISUN_DIAG_EAP_TLS_CREATE_CONF = 6,
    PYRONET_WISUN_DIAG_TLS_CONFIGURE_START = 7,
    PYRONET_WISUN_DIAG_TLS_LIB_CONNECT_FAIL = 8,
    PYRONET_WISUN_DIAG_TLS_PROCESS_RESULT = 9,
    PYRONET_WISUN_DIAG_TLS_SEND_TO_EAP = 10,
    PYRONET_WISUN_DIAG_EAP_TLS_SEND_CB = 11,
} pyronet_wisun_diag_event_t;

typedef struct pyronet_wisun_diag_snapshot
{
    uint32_t sequence;
    uint16_t event;
    int32_t a;
    int32_t b;
    int32_t c;
} pyronet_wisun_diag_snapshot_t;

void pyronet_wisun_diag_record(uint16_t event, int32_t a, int32_t b, int32_t c);
bool pyronet_wisun_diag_snapshot(pyronet_wisun_diag_snapshot_t *out);

#endif /* PYRONET_WISUN_DIAG_H */
