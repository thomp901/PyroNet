#ifndef __MBED_CONFIG_APP_DATA__
#define __MBED_CONFIG_APP_DATA__

#define MBED_CONF_MBED_MESH_APP_WISUN_NETWORK_SIZE                            1
#define MBED_CONF_NANOSTACK_HAL_EVENT_LOOP_THREAD_STACK_SIZE                  8192
#define MBED_CONF_MBED_MESH_API_CERTIFICATE_HEADER                            "wisun_certificates.h"
#define MBED_CONF_MBED_MESH_API_OWN_CERTIFICATE                               WISUN_CLIENT_CERTIFICATE
#define MBED_CONF_MBED_MESH_API_OWN_CERTIFICATE_KEY                           WISUN_CLIENT_KEY
#define MBED_CONF_MBED_MESH_API_ROOT_CERTIFICATE                              WISUN_ROOT_CERTIFICATE

#endif /* __MBED_CONFIG_APP_DATA__ */
