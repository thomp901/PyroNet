SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR ?= $(abspath ../../../../../../..)

include $(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/imports.mak

CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
LNK = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"

SDK_WISUN_EXAMPLE_DIR := $(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/examples/rtos/LP_CC1352P7_1/ti_wisunfan/ns_coap_node_src
SDK_WISUN_PROJECTSPEC := $(SDK_WISUN_EXAMPLE_DIR)/freertos/ticlang/ns_coap_node_src_LP_CC1352P7_1_freertos_ticlang.projectspec
SDK_WISUN_LINKER_CMD := $(SDK_WISUN_EXAMPLE_DIR)/freertos/ticlang/cc13x2x7_cc26x2x7.cmd

LOCAL_APP_DIR := ../../application
LOCAL_SRC_FILES := ../../freertos/main_freertos.c ../../sensor_ncp.c ../../swo_debug.c ../../uart_transport.c ../../pyronet_ncp.c ../../pyronet_wisun_globals.c ../../host/host_proto.c ../../host/host_uart.c ../../host/host_link.c ../../ncp/pyronet_mesh_decode.c ../../ncp/pyronet_ncp_events.c ../../ncp/pyronet_ncp_pending_tx.c ../../ncp/pyronet_ncp_rx.c ../../ncp/pyronet_ncp_state.c ../../ncp/pyronet_ncp_tx.c
LOCAL_TRACE_SRC := ../../wisun/platform/ns_trace.c
LOCAL_WISUN_OVERRIDE_SRC := ../../wisun/overrides/Security/protocols/tls_sec_prot/tls_sec_prot_lib.c \
    ../../wisun/overrides/mbedtls/src/aes_alt.c \
    ../../wisun/overrides/mbedtls/src/ctr_drbg.c
LOCAL_EVENTLOOP_TIMER_SRC := $(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack-eventloop/source/ns_timer.cpp
BUILD_DIR := build
NAME := sensor_ncp

SYSCONFIG_GUI_TOOL = $(dir $(SYSCONFIG_TOOL))sysconfig_gui$(suffix $(SYSCONFIG_TOOL))
SYSCFG_CMD_STUB = $(SYSCONFIG_TOOL) --compiler ticlang --product $(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/.metadata/product.json
SYSCFG_GUI_CMD_STUB = $(SYSCONFIG_GUI_TOOL) --compiler ticlang --product $(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/.metadata/product.json
SYSCFG_FILES := $(shell $(SYSCFG_CMD_STUB) --listGeneratedFiles --listReferencedFiles --output . ../../freertos/sensor_ncp.syscfg)

SYSCFG_C_FILES = $(filter %.c,$(SYSCFG_FILES))
SYSCFG_H_FILES = $(filter %.h,$(SYSCFG_FILES))
SYSCFG_OPT_FILES = $(filter %.opt,$(SYSCFG_FILES))

EXAMPLE_C_FILES := $(shell PROJECTSPEC="$(SDK_WISUN_PROJECTSPEC)" SDK="$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)" python3 -c "import os, xml.etree.ElementTree as ET; sdk=os.environ['SDK']; token='\$${COM_TI_SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR}'; excluded=('ncp_interface','/source/ti/ti_wisunfan/mbed_port/mbednanostack2tirtos/platform/ns_trace.c','/source/ti/ti_wisunfan/apps/udp_nanostack_mesh_tirf/main.c','/source/ti/ti_wisunfan/apps/udp_nanostack_mesh_tirf/application.c','/source/ti/ti_wisunfan/mbed_port/mbedtls2tirtos/mbedtls/mbed-crypto/src/aes_alt.c','/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Security/protocols/tls_sec_prot/tls_sec_prot_lib.c','/source/third_party/ti_wisunfan/mbedtls/src/ctr_drbg.c'); root=ET.parse(os.environ['PROJECTSPEC']).getroot(); print(' '.join([node.attrib.get('path', '').replace(token, sdk) for node in root.findall('.//file') if node.attrib.get('path', '').endswith('.c') and not any(ex in node.attrib.get('path', '') for ex in excluded)]))")

SRC_FILES := $(EXAMPLE_C_FILES) $(LOCAL_SRC_FILES) $(LOCAL_TRACE_SRC) $(LOCAL_WISUN_OVERRIDE_SRC)
CPP_FILES := $(LOCAL_EVENTLOOP_TIMER_SRC)

define sanitize
$(subst .,_,$(subst :,_,$(subst /,_,$(1))))
endef

OBJECTS := $(foreach src,$(SRC_FILES),$(BUILD_DIR)/$(call sanitize,$(src)).obj) \
           $(foreach src,$(CPP_FILES),$(BUILD_DIR)/$(call sanitize,$(src)).obj) \
           $(foreach src,$(SYSCFG_C_FILES),$(BUILD_DIR)/$(basename $(notdir $(src))).obj)

CFLAGS += -I. \
    -I../.. \
    -I$(LOCAL_APP_DIR) \
    -I../../wisun/include \
    -I../../wisun/platform \
    $(addprefix @,$(SYSCFG_OPT_FILES)) \
    -Oz \
    -munaligned-access \
    -fshort-enums \
    -fcommon \
    -funsigned-char \
    -mlittle-endian \
    -fdiagnostics-show-option \
    -ffunction-sections \
    -g \
    -fno-strict-aliasing \
    -flto \
    -DWISUN_RCP_ENABLE \
    -DEXCLUDE_TRACE \
    -DxSWITCH_NCP_TO_TRACE \
    -DTLS_SEC_PROT_LIB_USE_MBEDTLS_PLATFORM_MEMORY \
    -DxWISUN_AUTO_START \
    -DTI_WISUN_FAN_DEBUG \
    -DBoard_EXCLUDE_NVS_EXTERNAL_FLASH \
    -DDeviceFamily_CC13X2X7 \
    -DFREERTOS_SUPPORT \
    -DFREERTOS \
    -DNVOCMP_POSIX_MUTEX \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/mbed_port/ti-rf-driver/ti-rf-driver" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/apps/udp_nanostack_mesh_tirf" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/apps/common/include" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/common/nv" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/mbed_config/ws_router" \
    -include mbed_config.h \
    @$(LOCAL_APP_DIR)/defines/router.opts \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/posix/ticlang" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/mbed_port/ti-rf-driver/source" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/mbed_port/mbednanostack2tirtos" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/mbed_port/mbednanostack2tirtos/platform" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/mbed_port/mbedtls2tirtos" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/mbed_port/mbedtls2tirtos/platform" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/mbed_port/mbedtls2tirtos/mbedtls/mbed-crypto" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/frameworks/nanostack-libservice/mbed-client-libservice" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/frameworks/nanostack-libservice" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/frameworks/nanostack-libservice/mbed-client-libservice/platform" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/frameworks/mbed-client-randlib/mbed-client-randlib/platform" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/frameworks/mbed-client-randlib/mbed-client-randlib" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/frameworks/mbed-client-randlib" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/frameworks/mbed-trace" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/frameworks/mbed-coap" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/frameworks/mbed-coap/source/include" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Common_Protocols" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/nanostack" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/mdns" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/mdns/fnet" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/mdns/fnet/fnet_stack" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/mdns/fnet/fnet_stack/stack" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/mdns/fnet/fnet_stack/services/serial" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Core/include" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/6LoWPAN" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack-eventloop" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack-eventloop/nanostack-event-loop" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack-eventloop/nanostack-event-loop/platform" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/nanostack-hal-mbed-cmsis-rtos" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/mbed-mesh-api/source/include" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/mbed-mesh-api/mbed-mesh-api" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/mbed-mesh-api" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/coap-service/coap-service" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/coap-service/source/include" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/ti_wisunfan/mbedtls" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/ti_wisunfan/mbedtls/platform/inc" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/ti_wisunfan/mbedtls/inc" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/ti_wisunfan/mbedtls/platform/COMPONENT_PSA_SRV_IMPL/COMPONENT_NSPE" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/rcp" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/services" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/hal/assert" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/hal/appasrt" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/hal/crypto" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/hal/platform" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/hal/rf" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/high_level" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/low_level" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/common/osal_port" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/common" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/common/boards" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/common/util" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/common/inc" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/common/heapmgr" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/services/saddr" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/services/sdata" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/fh" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/inc" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/rom" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/inc/cc13xx" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/low_level/cc13xx" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/ti_wisunfan/wisunfan_mac/tracer" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/mbed-mesh-api/source" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/6LoWPAN/MAC" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/6LoWPAN/Thread" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Security/PANA" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/mdns/fnet/fnet_stack/services/mdns" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/fnv_hash" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/hmac" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/load_balance" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/libNET/src" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack-eventloop/source" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/ti_wisunfan/mbedtls/platform/COMPONENT_PSA_SRV_IMPL" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/whiteboard" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/6LoWPAN/ws" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/DHCPv6_client" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/libDHCPv6" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/utils" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/mbed/nanostack/sal-stack-nanostack/source/Service_Libs/pan_blacklist" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/devices/cc13x2x7_cc26x2x7" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/devices/cc13x2x7_cc26x2x7/driverlib" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/ti/devices/cc13x2x7_cc26x2x7/inc" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/kernel/freertos" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/freertos/include" \
    "-I$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source/third_party/freertos/portable/GCC/ARM_CM4F" \
    -gdwarf-3 \
    -mcpu=cortex-m4 \
    -march=armv7e-m \
    -mthumb \
    -mfloat-abi=hard \
    -mfpu=fpv4-sp-d16

CPPFLAGS += $(CFLAGS)

LFLAGS += "-L$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/source" \
    "-L$(SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR)/kernel" \
    -Wl,--diag_suppress=10063 \
    -Wl,--define=FREERTOS \
    ti_utils_build_linker.cmd.genlibs \
    "$(SDK_WISUN_LINKER_CMD)" \
    "-Wl,-m,$(NAME).map" \
    -Wl,--rom_model \
    -Wl,--warn_sections \
    "-L$(TICLANG_ARMCOMPILER)/lib" \
    -llibc.a

all: $(NAME).out

.INTERMEDIATE: syscfg
$(SYSCFG_FILES): syscfg
	@echo generation complete

syscfg: ../../freertos/sensor_ncp.syscfg
	@echo Generating configuration files...
	@$(SYSCFG_CMD_STUB) --output $(@D) $<

$(SYSCONFIG_GUI_TOOL):
	$(error $(dir $(SYSCONFIG_TOOL)) does not contain the GUI framework necessary to launch the SysConfig GUI. Please set SYSCONFIG_TOOL to a standalone SysConfig installation rather than one inside CCS)

syscfg-gui: ../../freertos/sensor_ncp.syscfg $(SYSCONFIG_GUI_TOOL)
	@echo Opening SysConfig GUI
	@$(SYSCFG_GUI_CMD_STUB) $<

define C_RULE
$(BUILD_DIR)/$(call sanitize,$(1)).obj: $(1) $(SYSCFG_H_FILES)
	@mkdir -p $(BUILD_DIR)
	@echo Building $$@
	@$(CC) $(CFLAGS) -c $$< -o $$@
endef

define SYSCFG_C_RULE
$(BUILD_DIR)/$(basename $(notdir $(1))).obj: $(1) $(SYSCFG_H_FILES)
	@mkdir -p $(BUILD_DIR)
	@echo Building $$@
	@$(CC) $(CFLAGS) -c $$< -o $$@
endef

define CPP_RULE
$(BUILD_DIR)/$(call sanitize,$(1)).obj: $(1) $(SYSCFG_H_FILES)
	@mkdir -p $(BUILD_DIR)
	@echo Building $$@
	@$(CC) $(CPPFLAGS) -c $$< -o $$@
endef

$(foreach c_file,$(SRC_FILES),$(eval $(call C_RULE,$(c_file))))
$(foreach cpp_file,$(CPP_FILES),$(eval $(call CPP_RULE,$(cpp_file))))
$(foreach c_file,$(SYSCFG_C_FILES),$(eval $(call SYSCFG_C_RULE,$(c_file))))

$(NAME).out: $(OBJECTS)
	@echo Linking $@
	@$(LNK) -Wl,-u,_c_int00 $(OBJECTS) $(LFLAGS) -o $(NAME).out

clean:
	@echo Cleaning...
	@$(RM) -r $(BUILD_DIR) > $(DEVNULL) 2>&1
	@$(RM) $(NAME).out > $(DEVNULL) 2>&1
	@$(RM) $(NAME).map > $(DEVNULL) 2>&1
	@$(RM) $(SYSCFG_FILES) > $(DEVNULL) 2>&1
