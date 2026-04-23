#ifndef NCP_PYRONET_NCP_RX_H
#define NCP_PYRONET_NCP_RX_H

#include <stdint.h>

#include "coap_service_api.h"

int pyronet_ncp_downlink_receive(int8_t service_id,
                                 uint8_t source_address[static 16],
                                 uint16_t source_port,
                                 sn_coap_hdr_s *request_ptr);

#endif /* NCP_PYRONET_NCP_RX_H */
