#ifndef NCP_PYRONET_NCP_LOCAL_H
#define NCP_PYRONET_NCP_LOCAL_H

#define PYRONET_COAP_PORT          5683U
#define PYRONET_EVENT_QUEUE_LEN    16U
#define PYRONET_PENDING_TX_LEN     32U
#define PYRONET_MAX_NEIGHBORS      32U
#define PYRONET_IPV6_ADDR_LEN      16U
#define PYRONET_ROUTER_ADDR_STR_LEN 48U

/*
 * These URI names are local transport constants for the current bring-up.
 * They intentionally do not define the shared MCU/NCP contract.
 */
#define PYRONET_COAP_UPLINK_URI    "uplink"
#define PYRONET_COAP_DOWNLINK_URI  "downlink"
#define PYRONET_COAP_LATERAL_URI   PYRONET_COAP_DOWNLINK_URI

#endif /* NCP_PYRONET_NCP_LOCAL_H */
