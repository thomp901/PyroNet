/***************************************************************************//**
 * @file sl_cli_command_table.c
 * @brief Declarations of relevant command structs for cli framework.
 * @version x.y.z
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include <stdlib.h>

#include "sl_cli_config.h"
#include "sl_cli_command.h"
#include "sl_cli_arguments.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *****************************   TEMPLATED FILE   ******************************
 ******************************************************************************/

/*******************************************************************************
 * Example syntax (.slcc or .slcp) for populating this file:
 *
 *   template_contribution:
 *     - name: cli_command          # Register a command
 *       value:
 *         name: status             # Name of command
 *         handler: status_command  # Function to be called. Must be defined
 *         help: "Prints status"    # Optional help description
 *         shortcuts:               # Optional shorcut list
 *           - name: st
 *         argument:                # Argument list, if apliccable
 *           - type: uint8          # Variable type
 *             help: "Channel"      # Optional description
 *           - type: string
 *             help: "Text"
 *     - name: cli_group            # Register a group
 *       value:
 *         name: shell              # Group name
 *         help: "Shell commands"   # Optional help description
 *         shortcuts:               # Optional shorcuts
 *           - name: sh
 *     - name: cli_command
 *       value:
 *         name: repeat
 *         handler: repeat_cmd
 *         help: "Repeat commands"
 *         shortcuts:
 *           - name: r
 *           - name: rep
 *         group: shell            # Associate command with group
 *         argument:
 *           - type: string
 *             help: "Text"
 *           - type: additional
 *             help: "More text"
 *
 * For subgroups, an optional unique id can be used to allow a particular name to
 * be used more than once. In the following case, from the command line the
 * following commands are available:
 *
 * >  root_1 shell status
 * >  root_2 shell status
 *
 *     - name: cli_group            # Register a group
 *       value:
 *         name: root_1             # Group name
 *
 *     - name: cli_group            # Register a group
 *       value:
 *         name: root_2             # Group name
 *
 *    - name: cli_group             # Register a group
 *       value:
 *         name: shell              # Group name
 *         id: shell_root_1         # Optional unique id for group
 *         group: root_1            # Add group to root_1 group
 *
 *    - name: cli_group             # Register a group
 *       value:
 *         name: shell              # Group name
 *         id: shell_root_2         # Optional unique id for group
 *         group: root_2            # Add group to root_1 group
 *
 *    - name: cli_command           # Register a command
 *       value:
 *         name: status
 *         handler: status_1
 *         group: shell_root_1      # id of subgroup
 *
 *    - name: cli_command           # Register a command
 *       value:
 *         name: status
 *         handler: status_2
 *         group: shell_root_2      # id of subgroup
 *
 ******************************************************************************/

// Provide function declarations
void app_set(sl_cli_command_arg_t *arguments);
void app_get(sl_cli_command_arg_t *arguments);
void app_save(sl_cli_command_arg_t *arguments);
void app_reset(sl_cli_command_arg_t *arguments);
void app_about(sl_cli_command_arg_t *arguments);
void app_join_fan10(sl_cli_command_arg_t *arguments);
void app_join_fan11(sl_cli_command_arg_t *arguments);
void app_join_explicit(sl_cli_command_arg_t *arguments);
void app_join_ids(sl_cli_command_arg_t *arguments);
void app_join_custom_fsk(sl_cli_command_arg_t *arguments);
void app_join_custom_ofdm(sl_cli_command_arg_t *arguments);
void app_join_custom_oqpsk(sl_cli_command_arg_t *arguments);
void app_disconnect(sl_cli_command_arg_t *arguments);
void app_ping(sl_cli_command_arg_t *arguments);
void app_tcp_client(sl_cli_command_arg_t *arguments);
void app_tcp_server(sl_cli_command_arg_t *arguments);
void app_udp_client(sl_cli_command_arg_t *arguments);
void app_udp_server(sl_cli_command_arg_t *arguments);
void app_socket_close(sl_cli_command_arg_t *arguments);
void app_socket_read(sl_cli_command_arg_t *arguments);
void app_socket_write(sl_cli_command_arg_t *arguments);
void app_socket_writeto(sl_cli_command_arg_t *arguments);
void app_pyronet_spoof(sl_cli_command_arg_t *arguments);
void app_socket_list(sl_cli_command_arg_t *arguments);
void app_socket_set_option(sl_cli_command_arg_t *arguments);
void app_mac_allow(sl_cli_command_arg_t *arguments);
void app_mac_deny(sl_cli_command_arg_t *arguments);
void app_socket_get_option(sl_cli_command_arg_t *arguments);
void app_clear_credential_cache(sl_cli_command_arg_t *arguments);
void app_reset_statistics(sl_cli_command_arg_t *arguments);
void app_set_trace_level(sl_cli_command_arg_t *arguments);
void app_set_lfn_support(sl_cli_command_arg_t *arguments);
void app_mode_switch(sl_cli_command_arg_t *arguments);
void app_concurrent_detection(sl_cli_command_arg_t *arguments);
void app_trigger_frame(sl_cli_command_arg_t *arguments);
void app_getsockname(sl_cli_command_arg_t *arguments);
void app_getpeername(sl_cli_command_arg_t *arguments);
void app_set_leaf(sl_cli_command_arg_t *arguments);
void app_set_direct_connect_state(sl_cli_command_arg_t *arguments);
void app_accept_direct_connect_link(sl_cli_command_arg_t *arguments);
void app_set_phy_sensitivity(sl_cli_command_arg_t *arguments);
void app_reset_duty_cycle(sl_cli_command_arg_t *arguments);
void app_set_event_log_filter(sl_cli_command_arg_t *arguments);
void app_event_log(sl_cli_command_arg_t *arguments);
void app_rftest_start_stream(sl_cli_command_arg_t *arguments);
void app_rftest_stop_stream(sl_cli_command_arg_t *arguments);
void app_rftest_start_tone(sl_cli_command_arg_t *arguments);
void app_rftest_stop_tone(sl_cli_command_arg_t *arguments);
void app_rftest_set_tx_power(sl_cli_command_arg_t *arguments);

// Command structs. Names are in the format : cli_cmd_{command group name}_{command name}
// In order to support hyphen in command and group name, every occurence of it while
// building struct names will be replaced by "_hyphen_"
static const sl_cli_command_info_t cli_cmd_wisun_set = \
  SL_CLI_COMMAND(app_set,
                 "Set a variable",
                  "empty | help | [string] Key [string] Value" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_WILDCARD, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_get = \
  SL_CLI_COMMAND(app_get,
                 "Get a variable",
                  "empty | help | [string] Key" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_WILDCARD, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_save = \
  SL_CLI_COMMAND(app_save,
                 "Save variables to non-volatile storage",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_reset = \
  SL_CLI_COMMAND(app_reset,
                 "Reset variables to default settings",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd__about = \
  SL_CLI_COMMAND(app_about,
                 "Get info about the running app",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_join_fan10 = \
  SL_CLI_COMMAND(app_join_fan10,
                 "Connect to a Wi-SUN network using FAN1.0 settings: w j10",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_join_fan11 = \
  SL_CLI_COMMAND(app_join_fan11,
                 "Connect to a Wi-SUN network using FAN1.1 settings: w j11",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_join_explicit = \
  SL_CLI_COMMAND(app_join_explicit,
                 "Connect to a Wi-SUN network using explicit PHY settings: w je",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_join_ids = \
  SL_CLI_COMMAND(app_join_ids,
                 "Connect to a Wi-SUN network using radio conf ids: w ji",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_join_custom_fsk = \
  SL_CLI_COMMAND(app_join_custom_fsk,
                 "Connect to a Wi-SUN network using custom FSK PHY settings: w jcf",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_join_custom_ofdm = \
  SL_CLI_COMMAND(app_join_custom_ofdm,
                 "Connect to a Wi-SUN network using custom OFDM PHY settings: w jcof",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_join_custom_oqpsk = \
  SL_CLI_COMMAND(app_join_custom_oqpsk,
                 "Connect to a Wi-SUN network using custom OQPSK PHY settings: w jcoq",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_disconnect = \
  SL_CLI_COMMAND(app_disconnect,
                 "Disconnect from the Wi-SUN network: w d",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_ping = \
  SL_CLI_COMMAND(app_ping,
                 "Ping a remote host: w p",
                  "Remote address" SL_CLI_UNIT_SEPARATOR "packet size in byte" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_UINT16OPT, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_tcp_client = \
  SL_CLI_COMMAND(app_tcp_client,
                 "Open a TCP client socket: w tc",
                  "Remote address" SL_CLI_UNIT_SEPARATOR "Remote port" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_UINT16, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_tcp_server = \
  SL_CLI_COMMAND(app_tcp_server,
                 "Open a TCP server socket: w ts",
                  "Local port" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT16, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_udp_client = \
  SL_CLI_COMMAND(app_udp_client,
                 "Open a UDP client socket: w uc",
                  "Remote address" SL_CLI_UNIT_SEPARATOR "Remote port" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_UINT16, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_udp_server = \
  SL_CLI_COMMAND(app_udp_server,
                 "Open a UDP server socket: w us",
                  "Local port" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT16, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_socket_close = \
  SL_CLI_COMMAND(app_socket_close,
                 "Close a socket: w sc",
                  "Socket Id" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT32, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_socket_read = \
  SL_CLI_COMMAND(app_socket_read,
                 "Read from a socket: w sr",
                  "Socket Id" SL_CLI_UNIT_SEPARATOR "Amount of bytes to read" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT32, SL_CLI_ARG_UINT16, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_socket_write = \
  SL_CLI_COMMAND(app_socket_write,
                 "Write to a socket: w sw",
                  "Socket Id" SL_CLI_UNIT_SEPARATOR "Data to write" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT32, SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_socket_writeto = \
  SL_CLI_COMMAND(app_socket_writeto,
                 "Write to a socket to a specific host: w swt",
                  "Socket Id" SL_CLI_UNIT_SEPARATOR "Remote address" SL_CLI_UNIT_SEPARATOR "Remote port" SL_CLI_UNIT_SEPARATOR "Data to write" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT32, SL_CLI_ARG_STRING, SL_CLI_ARG_UINT16, SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_pyronet_spoof = \
  SL_CLI_COMMAND(app_pyronet_spoof,
                 "Send hard-coded PyroNet CSP spoof packets: w ps",
                  "Remote address" SL_CLI_UNIT_SEPARATOR "Remote port" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_UINT16, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_socket_list = \
  SL_CLI_COMMAND(app_socket_list,
                 "List open sockets: w sl",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_socket_set_option = \
  SL_CLI_COMMAND(app_socket_set_option,
                 "Set a socket option: w ss",
                  "Socket Id" SL_CLI_UNIT_SEPARATOR "Option" SL_CLI_UNIT_SEPARATOR "Option data" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT32, SL_CLI_ARG_STRING, SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_mac_allow = \
  SL_CLI_COMMAND(app_mac_allow,
                 "Add a MAC address to the list of allowed addresses, (FF:FF:FF:FF:FF:FF:FF:FF) clears the whitelist: w ma",
                  "MAC address" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_mac_deny = \
  SL_CLI_COMMAND(app_mac_deny,
                 "Add a MAC address to the list of denied addresses: w md",
                  "MAC address" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_socket_get_option = \
  SL_CLI_COMMAND(app_socket_get_option,
                 "Get a socket option: w sg",
                  "empty | [uint32] Socket Id [string] Option" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_WILDCARD, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_clear_credential_cache = \
  SL_CLI_COMMAND(app_clear_credential_cache,
                 "Clear the credential cache: w ccc",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_reset_statistics = \
  SL_CLI_COMMAND(app_reset_statistics,
                 "Reset the statistics: w rs",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_set_trace_level = \
  SL_CLI_COMMAND(app_set_trace_level,
                 "Set the trace level: w tl",
                  "List of ID,level; such as 1,none;4,info to configure group ID 1 with no log and group ID 4 with INFO level. Can also use all,info to set all groups to INFO level" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_set_lfn_support = \
  SL_CLI_COMMAND(app_set_lfn_support,
                 "configure LFN support",
                  "lfn_limit: Number of locally supported LFN" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_mode_switch = \
  SL_CLI_COMMAND(app_mode_switch,
                 "configure mode witch: w ms",
                  "mode: 0=disabled, 1=enabled, 2=default" SL_CLI_UNIT_SEPARATOR "PhyModeId" SL_CLI_UNIT_SEPARATOR "neighbor MAC address (optional)" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_UINT8, SL_CLI_ARG_STRINGOPT, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_concurrent_detection = \
  SL_CLI_COMMAND(app_concurrent_detection,
                 "configure concurrent detection: w cd",
                  "enable: 0=disabled, 1=enabled" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_trigger = \
  SL_CLI_COMMAND(app_trigger_frame,
                 "Trigger the transmission of a frame",
                  "frame type" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_getsockname = \
  SL_CLI_COMMAND(app_getsockname,
                 "Get socket's local Ip address",
                  "Socket Id" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT32, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_getpeername = \
  SL_CLI_COMMAND(app_getpeername,
                 "Get socket's peer Ip address",
                  "Socket Id" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT32, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_set_leaf = \
  SL_CLI_COMMAND(app_set_leaf,
                 "Set Leaf behavior",
                  "is_leaf: 1 to act as RPL leaf" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_set_direct_connect_state = \
  SL_CLI_COMMAND(app_set_direct_connect_state,
                 "Enable/disable Direct Connect",
                  "is_enabled: 1 to enable Direct Connect" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_accept_direct_connect_link = \
  SL_CLI_COMMAND(app_accept_direct_connect_link,
                 "Accept a Direct Connect link",
                  "Client link local address" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_set_phy_sens = \
  SL_CLI_COMMAND(app_set_phy_sensitivity,
                 "Set the sensitivity for the given PHY mode ID",
                  "PHY mode ID" SL_CLI_UNIT_SEPARATOR "Sensitivity in dBm" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT8, SL_CLI_ARG_INT16, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_reset_duty_cycle = \
  SL_CLI_COMMAND(app_reset_duty_cycle,
                 "Reset the duty cycle counters",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_set_event_log_filter = \
  SL_CLI_COMMAND(app_set_event_log_filter,
                 "Set an event log filter: w sf",
                  "Neighbor MAC address, FF:FF:FF:FF:FF:FF:FF:FF for default filtering" SL_CLI_UNIT_SEPARATOR "Event filter mask written in hex format (for example {02} for received packets)" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_HEX, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_wisun_event_log = \
  SL_CLI_COMMAND(app_event_log,
                 "Read the event_log: w el",
                  "Neighbor MAC address" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_rftest_start_stream = \
  SL_CLI_COMMAND(app_rftest_start_stream,
                 "Start an RF test stream: rft strs",
                  "Channel Id" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT16, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_rftest_stop_stream = \
  SL_CLI_COMMAND(app_rftest_stop_stream,
                 "Stop an RF test stream: rft stps",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_rftest_start_tone = \
  SL_CLI_COMMAND(app_rftest_start_tone,
                 "Start an RF test tone: rft strt",
                  "Channel Id" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_UINT16, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_rftest_stop_tone = \
  SL_CLI_COMMAND(app_rftest_stop_tone,
                 "Stop an RF test tone: rft stpt",
                  "",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cli_cmd_rftest_set_tx_power = \
  SL_CLI_COMMAND(app_rftest_set_tx_power,
                 "Set Tx Power for RF test: rft sttx",
                  "Tx Power (dBm)" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_INT8, SL_CLI_ARG_END, });


// Create group command tables and structs if cli_groups given
// in template. Group name is suffixed with _group_table for tables
// and group commands are cli_cmd_grp_( group name )
static const sl_cli_command_entry_t wisun_group_table[] = {
  { "set", &cli_cmd_wisun_set, false },
  { "s", &cli_cmd_wisun_set, true },
  { "get", &cli_cmd_wisun_get, false },
  { "g", &cli_cmd_wisun_get, true },
  { "save", &cli_cmd_wisun_save, false },
  { "a", &cli_cmd_wisun_save, true },
  { "reset", &cli_cmd_wisun_reset, false },
  { "r", &cli_cmd_wisun_reset, true },
  { "join_fan10", &cli_cmd_wisun_join_fan10, false },
  { "j10", &cli_cmd_wisun_join_fan10, true },
  { "join_fan11", &cli_cmd_wisun_join_fan11, false },
  { "j11", &cli_cmd_wisun_join_fan11, true },
  { "join_explicit", &cli_cmd_wisun_join_explicit, false },
  { "je", &cli_cmd_wisun_join_explicit, true },
  { "join_ids", &cli_cmd_wisun_join_ids, false },
  { "ji", &cli_cmd_wisun_join_ids, true },
  { "join_custom_fsk", &cli_cmd_wisun_join_custom_fsk, false },
  { "jcf", &cli_cmd_wisun_join_custom_fsk, true },
  { "join_custom_ofdm", &cli_cmd_wisun_join_custom_ofdm, false },
  { "jcof", &cli_cmd_wisun_join_custom_ofdm, true },
  { "join_custom_oqpsk", &cli_cmd_wisun_join_custom_oqpsk, false },
  { "jcoq", &cli_cmd_wisun_join_custom_oqpsk, true },
  { "disconnect", &cli_cmd_wisun_disconnect, false },
  { "d", &cli_cmd_wisun_disconnect, true },
  { "ping", &cli_cmd_wisun_ping, false },
  { "p", &cli_cmd_wisun_ping, true },
  { "tcp_client", &cli_cmd_wisun_tcp_client, false },
  { "tc", &cli_cmd_wisun_tcp_client, true },
  { "tcp_server", &cli_cmd_wisun_tcp_server, false },
  { "ts", &cli_cmd_wisun_tcp_server, true },
  { "udp_client", &cli_cmd_wisun_udp_client, false },
  { "uc", &cli_cmd_wisun_udp_client, true },
  { "udp_server", &cli_cmd_wisun_udp_server, false },
  { "us", &cli_cmd_wisun_udp_server, true },
  { "socket_close", &cli_cmd_wisun_socket_close, false },
  { "sc", &cli_cmd_wisun_socket_close, true },
  { "socket_read", &cli_cmd_wisun_socket_read, false },
  { "sr", &cli_cmd_wisun_socket_read, true },
  { "socket_write", &cli_cmd_wisun_socket_write, false },
  { "sw", &cli_cmd_wisun_socket_write, true },
  { "socket_writeto", &cli_cmd_wisun_socket_writeto, false },
  { "swt", &cli_cmd_wisun_socket_writeto, true },
  { "pyronet_spoof", &cli_cmd_wisun_pyronet_spoof, false },
  { "ps", &cli_cmd_wisun_pyronet_spoof, true },
  { "socket_list", &cli_cmd_wisun_socket_list, false },
  { "sl", &cli_cmd_wisun_socket_list, true },
  { "socket_set_option", &cli_cmd_wisun_socket_set_option, false },
  { "ss", &cli_cmd_wisun_socket_set_option, true },
  { "mac_allow", &cli_cmd_wisun_mac_allow, false },
  { "ma", &cli_cmd_wisun_mac_allow, true },
  { "mac_deny", &cli_cmd_wisun_mac_deny, false },
  { "md", &cli_cmd_wisun_mac_deny, true },
  { "socket_get_option", &cli_cmd_wisun_socket_get_option, false },
  { "sg", &cli_cmd_wisun_socket_get_option, true },
  { "clear_credential_cache", &cli_cmd_wisun_clear_credential_cache, false },
  { "ccc", &cli_cmd_wisun_clear_credential_cache, true },
  { "reset_statistics", &cli_cmd_wisun_reset_statistics, false },
  { "rs", &cli_cmd_wisun_reset_statistics, true },
  { "set_trace_level", &cli_cmd_wisun_set_trace_level, false },
  { "tl", &cli_cmd_wisun_set_trace_level, true },
  { "set_lfn_support", &cli_cmd_wisun_set_lfn_support, false },
  { "mode_switch", &cli_cmd_wisun_mode_switch, false },
  { "ms", &cli_cmd_wisun_mode_switch, true },
  { "concurrent_detection", &cli_cmd_wisun_concurrent_detection, false },
  { "cd", &cli_cmd_wisun_concurrent_detection, true },
  { "trigger", &cli_cmd_wisun_trigger, false },
  { "tx", &cli_cmd_wisun_trigger, true },
  { "getsockname", &cli_cmd_wisun_getsockname, false },
  { "getpeername", &cli_cmd_wisun_getpeername, false },
  { "set_leaf", &cli_cmd_wisun_set_leaf, false },
  { "set_direct_connect_state", &cli_cmd_wisun_set_direct_connect_state, false },
  { "accept_direct_connect_link", &cli_cmd_wisun_accept_direct_connect_link, false },
  { "set_phy_sens", &cli_cmd_wisun_set_phy_sens, false },
  { "reset_duty_cycle", &cli_cmd_wisun_reset_duty_cycle, false },
  { "", &cli_cmd_wisun_reset_duty_cycle, true },
  { "", &cli_cmd_wisun_reset_duty_cycle, true },
  { "", &cli_cmd_wisun_reset_duty_cycle, true },
  { "set_event_log_filter", &cli_cmd_wisun_set_event_log_filter, false },
  { "sf", &cli_cmd_wisun_set_event_log_filter, true },
  { "event_log", &cli_cmd_wisun_event_log, false },
  { "el", &cli_cmd_wisun_event_log, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_wisun = \
  SL_CLI_COMMAND_GROUP(wisun_group_table, "Wi-SUN commands");

static const sl_cli_command_entry_t rftest_group_table[] = {
  { "start_stream", &cli_cmd_rftest_start_stream, false },
  { "[strs] ", &cli_cmd_rftest_start_stream, true },
  { "stop_stream", &cli_cmd_rftest_stop_stream, false },
  { "stps", &cli_cmd_rftest_stop_stream, true },
  { "start_tone", &cli_cmd_rftest_start_tone, false },
  { "strt", &cli_cmd_rftest_start_tone, true },
  { "stop_tone", &cli_cmd_rftest_stop_tone, false },
  { "stpt", &cli_cmd_rftest_stop_tone, true },
  { "set_tx_power", &cli_cmd_rftest_set_tx_power, false },
  { "sttx", &cli_cmd_rftest_set_tx_power, true },
  { NULL, NULL, false },
};
static const sl_cli_command_info_t cli_cmd_grp_rftest = \
  SL_CLI_COMMAND_GROUP(rftest_group_table, "RF test commands");

// Create root command table
const sl_cli_command_entry_t sl_cli_default_command_table[] = {
  { "about", &cli_cmd__about, false },
  { "a", &cli_cmd__about, true },
  { "wisun", &cli_cmd_grp_wisun, false },
  { "w", &cli_cmd_grp_wisun, true },
  { "rftest", &cli_cmd_grp_rftest, false },
  { "rft", &cli_cmd_grp_rftest, true },
  { NULL, NULL, false },
};


#ifdef __cplusplus
}
#endif
