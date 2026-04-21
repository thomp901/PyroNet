/*
 * Local wrapper for the TI Wi-SUN API header.
 *
 * The upstream header declares ws_pan_information_t without pulling in the
 * Wi-SUN type definition first. Keep the SDK source untouched and satisfy the
 * dependency here.
 */

#ifndef PYRONET_LOCAL_API_MAC_WRAPPER_H
#define PYRONET_LOCAL_API_MAC_WRAPPER_H

#include "6LoWPAN/ws/ws_common_defines.h"
#include_next "api_mac.h"

#endif /* PYRONET_LOCAL_API_MAC_WRAPPER_H */
