#include "sl_event_handler.h"

#include "sl_board_init.h"
#include "sl_clock_manager.h"
#include "pa_conversions_efr32.h"
#include "sl_rail_util_pti.h"
#include "sl_board_control.h"
#include "sl_gpio.h"
#include "sl_iostream_rtt.h"
#include "sl_mbedtls.h"
#include "sl_wsrcp.h"
#include "psa/crypto.h"
#include "sl_se_manager.h"
#include "sl_iostream_init_instances.h"
#include "cmsis_os2.h"
#include "nvm3_default.h"
#include "sl_iostream_handles.h"
#include "sl_rail_util_rf_path_switch.h"

void sli_driver_permanent_allocation(void)
{
}

void sli_service_permanent_allocation(void)
{
}

void sli_stack_permanent_allocation(void)
{
}

void sli_internal_permanent_allocation(void)
{
}

void sl_platform_init(void)
{
  sl_board_preinit();
  sl_clock_manager_runtime_init();
  sl_board_init();
  nvm3_initDefault();
}

void sli_internal_init_early(void)
{
}

void sl_kernel_start(void)
{
  osKernelStart();
}

void sl_driver_init(void)
{
  sl_gpio_init();
}

void sl_service_init(void)
{
  sl_board_configure_vcom();
  sl_mbedtls_init();
  psa_crypto_init();
  sl_se_init();
  sl_iostream_init_instances_stage_1();
  sl_iostream_init_instances_stage_2();
}

void sl_stack_init(void)
{
  sl_rail_util_pa_init();
  sl_rail_util_pti_init();
  sl_rail_util_rf_path_switch_init();
}

void sl_internal_app_init(void)
{
  wisun_rcp_init();
}

void sl_iostream_init_instances_stage_1(void)
{
  sl_iostream_rtt_init();
}

void sl_iostream_init_instances_stage_2(void)
{
  sl_iostream_set_console_instance();
}

