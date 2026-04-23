#ifndef NCP_PYRONET_NCP_PENDING_TX_H
#define NCP_PYRONET_NCP_PENDING_TX_H

#include <stdbool.h>
#include <stdint.h>

#include "pyronet_ncp_local.h"

bool pyronet_ncp_pending_tx_init(void);
bool pyronet_ncp_pending_tx_reserve(uint8_t *slot_index_out,
                                    uint16_t port,
                                    const uint8_t destination[static PYRONET_IPV6_ADDR_LEN],
                                    uint8_t request_type,
                                    uint8_t detail);
void pyronet_ncp_pending_tx_release(uint8_t slot_index);
bool pyronet_ncp_pending_tx_commit(uint8_t slot_index, uint16_t msg_id);
bool pyronet_ncp_pending_tx_match_and_consume(uint16_t msg_id,
                                              const uint8_t destination[static PYRONET_IPV6_ADDR_LEN],
                                              uint16_t port,
                                              uint8_t *request_type_out,
                                              uint8_t *detail_out);

#endif /* NCP_PYRONET_NCP_PENDING_TX_H */
