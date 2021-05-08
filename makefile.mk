#
# Copyright 2018, Cypress Semiconductor Corporation or a subsidiary of Cypress Semiconductor 
#  Corporation. All rights reserved. This software, including source code, documentation and  related 
# materials ("Software"), is owned by Cypress Semiconductor  Corporation or one of its 
#  subsidiaries ("Cypress") and is protected by and subject to worldwide patent protection  
# (United States and foreign), United States copyright laws and international treaty provisions. 
# Therefore, you may use this Software only as provided in the license agreement accompanying the 
# software package from which you obtained this Software ("EULA"). If no EULA applies, Cypress 
# hereby grants you a personal, nonexclusive, non-transferable license to  copy, modify, and 
# compile the Software source code solely for use in connection with Cypress's  integrated circuit 
# products. Any reproduction, modification, translation, compilation,  or representation of this 
# Software except as specified above is prohibited without the express written permission of 
# Cypress. Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO  WARRANTY OF ANY KIND, EXPRESS 
# OR IMPLIED, INCLUDING,  BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY 
# AND FITNESS FOR A PARTICULAR PURPOSE. Cypress reserves the right to make changes to 
# the Software without notice. Cypress does not assume any liability arising out of the application 
# or use of the Software or any product or circuit  described in the Software. Cypress does 
# not authorize its products for use in any products where a malfunction or failure of the 
# Cypress product may reasonably be expected to result  in significant property damage, injury 
# or death ("High Risk Product"). By including Cypress's product in a High Risk Product, the 
#  manufacturer of such system or application assumes  all risk of such use and in doing so agrees 
# to indemnify Cypress against all liability.
#

NAME := mesh_provisioner_node

# Mesh debug: enable (or pass via make target) to use debug/trace mesh libraries
# NOTE:  20706 limited to only 1 of mesh_models or mesh_core debug libraries
#        - do not enable both simultaneously for 20706
# MESH_MODELS_DEBUG := 1
# MESH_CORE_DEBUG := 1
# MESH_PROVISIONER_DEBUG := 1
#C_FLAGS += -DMESH_CORE_DONT_USE_TRACE_ENCODER

#C_FLAGS += -DPTS

ifeq ($(USE_REMOTE_PROVISION),1)
C_FLAGS += -DUSE_REMOTE_PROVISION  
C_FLAGS += -DREMOTE_PROVISION_SERVER_SUPPORTED
#C_FLAGS += -DREMOTE_PROVISION_OVER_GATT_SUPPORTED  #确定手机范围内的设备是使用GATT入网还是通过ADV通过代理节点入网 加此宏是通过GATT入网
endif
C_FLAGS += -DMESH_DFU_SUPPORTED
# C_FLAGS += -DREMOTE_PROVISION_OVER_GATT_SUPPORTED
# C_FLAGS += -DGATT_PROXY_CLIENT_SUPPORTED

C_FLAGS += -DENABLE_SELF_CONFIG_MODEL
C_FLAGS += -DUSE_VENDOR_MODEL
# C_FLAGS += -DBLE_MESH_DFU
# C_FLAGS += -DMESH_FW_PROVIDER_SUPPORTED


INCS += ../../common/libraries/fw_upgrade_lib/

ifdef MESH_START_SYNC
C_FLAGS += -DMESH_START_SYNC=$(MESH_START_SYNC)
ifneq ($(MESH_START_SYNC),0)
APP_PATCHES_AND_LIBS += wiced_hidd_lib.a
APPLIBDIR := ../../common/libraries/hidd_lib
include $(APPLIBDIR)/makefile.mk
endif
else
C_FLAGS += -DMESH_START_SYNC=0
endif

########################################################################
# Mesh models enabled in mesh_provision_client. All client apps may not
# fit on all platforms. Add Application sources required for the client here.
########################################################################
APP_SRC  = src_provisioner/mesh_provisioner_node.c
#APP_SRC += src_provisioner/wiced_mesh_db.c
#APP_SRC += src_provisioner/wiced_mesh_client.c
APP_SRC += src_provisioner/mesh_app_self_config.c
APP_SRC += src_provisioner/mesh_application.c
APP_SRC += src_provisioner/mesh_app_provision_server.c
APP_SRC += src_provisioner/mesh_app_hci.c
APP_SRC += src_provisioner/mesh_app_gatt.c
APP_SRC += src_provisioner/wiced_bt_cfg.c
APP_SRC += src_provisioner/mesh_app_dfu.c
APP_SRC += src_provisioner/ecdsa256_pub.c
APP_SRC += src_provisioner/wiced_bt_cfg.c
APP_SRC += src_provisioner/wiced_platform_pin_config.c


APP_SRC += src_light/app.c
APP_SRC += src_light/storage.c
APP_SRC += src_light/pwm_control.c
APP_SRC += src_light/log.c
APP_SRC += src_light/vendor.c
APP_SRC += src_light/AES.c
APP_SRC += src_light/adv_pack.c
#APP_SRC += src_light/AndonPair.c
#APP_SRC += src_light/standard_model.c
APP_SRC += src_light/light_model.c
APP_SRC += src_light/raw_flash.c
#APP_SRC += src_light/record.c
APP_SRC += src_light/llog.c
APP_SRC += src_light/tooling.c
# APP_SRC += src_light/sflash_test.c
APP_SRC += src_light/andon_server.c

APP_SRC += src_light/systimer.c
APP_SRC += src_light/DH.c
APP_SRC += src_light/wyzebase64.c
APP_SRC += src_light/xxtea_F.c
APP_SRC += src_light/wyze_md5.c
APP_SRC += src_light/WyzeService.c
APP_SRC += src_light/LightPackage.c
APP_SRC += src_light/mylib.c
#APP_SRC += src_light/schedule_learn.c
APP_SRC += src_light/lcd_driver.c


########################################################################
# Component(s) needed
########################################################################
#$(NAME)_COMPONENTS := mesh_app_lib.a
$(NAME)_COMPONENTS := fw_upgrade_lib.a
$(NAME)_COMPONENTS += factory_config_lib.a
$(NAME)_COMPONENTS += gatt_utils_lib.a

ifeq ($(LIGHTAI),configLIGHTAIANDONMODE) 
C_FLAGS += -DLIGHTAI=$(LIGHTAI)
APP_SRC += src_light/schedule_learn.c
else 
ifeq ($(LIGHTAI),configLIGHTAIWYZEMODE) 
$(NAME)_COMPONENTS += aiparser_lib.a
C_FLAGS += -DLIGHTAI=$(LIGHTAI)
endif
endif



ifeq ($(MESH_MODELS_DEBUG),1)
$(NAME)_COMPONENTS += mesh_models_lib_d.a
else
$(NAME)_COMPONENTS += mesh_models_lib.a
endif
ifeq ($(MESH_CORE_DEBUG),1)
$(NAME)_COMPONENTS += mesh_core_lib-d.a
else
#$(NAME)_COMPONENTS += mesh_core_lib_getway.a
#$(NAME)_COMPONENTS += mesh_core_lib_configtimeouttest.a
$(NAME)_COMPONENTS += mesh_core_lib.a
#$(NAME)_COMPONENTS += mesh_core_lib_0806.a
#$(NAME)_COMPONENTS += mesh_core_lib_0820.a
#$(NAME)_COMPONENTS += mesh_core_lib_0514.a
endif
ifeq ($(MESH_PROVISIONER_DEBUG),1)
$(NAME)_COMPONENTS += mesh_provisioner_lib-d.a
else
$(NAME)_COMPONENTS += mesh_provisioner_lib.a
endif

ifeq ($(BLD),A_20719B0)
APP_PATCHES_AND_LIBS := multi_adv_patch_lib.a
#C_FLAGS += -DNO_MTU_REQ
else ifeq ($(CHIP),20703)
APP_PATCHES_AND_LIBS := rtc_lib.a
APP_PATCHES_AND_LIBS += wiced_bt_mesh.a
endif

ifdef LIGHT
C_FLAGS += -DLIGHT=$(LIGHT)
C_FLAGS += -DSFLASH_SIZE_8M_BITS
endif

ifeq ($(ANDON_TEST),1)
C_FLAGS += -DANDON_TEST=1
# C_FLAGS += -DWICED_BT_TRACE_ENABLE
else
C_FLAGS += -DANDON_TEST=0
endif

# ifdef _DEBUG
# #C_FLAGS += -DOTA_UPGRADE_DEBUG
# C_FLAGS += -D_ANDONDEBUG=1
# C_FLAGS += -DLOGLEVEL=$(_DEBUG)
# endif

########################################################################
# C flags
########################################################################
C_FLAGS += -DMESH_PROVISION_CLIENT
#TODO 灵动开关的检测目前未添加 开发板上无相关电路1
ifneq ($(CHIP_SCHEME),CHIP_DEVKIT)
C_FLAGS += -D_ANDONDEBUG=1
C_FLAGS += -DLOGLEVEL=LOGLEVEL_INFO 
C_FLAGS += -DCHIP_SCHEME=$(CHIP_SCHEME)
C_FLAGS += -DCHIP_EVK=0
C_FLAGS += -DCHECK_POWEROFF_VALUE=1
C_FLAGS += -DCHECK_POWEROFF_INTT=0
else 
C_FLAGS += -D_ANDONDEBUG=1
C_FLAGS += -DLOGLEVEL=LOGLEVEL_DEBUG
C_FLAGS += -DCHIP_SCHEME=CHIP_DEVKIT
C_FLAGS += -DCHIP_EVK=1
C_FLAGS += -DCHECK_POWEROFF_VALUE=1
C_FLAGS += -DCHECK_POWEROFF_INTT=0
C_FLAGS += -DWICED_BT_TRACE_ENABLE
endif

# define this for 2 chip solution or for testing over WICED HCI
# C_FLAGS += -DHCI_CONTROL
C_FLAGS += -DTRANSPORT_BUFFER_SIZE=400
#C_FLAGS += -D_DEB_ENABLE_HCI_TRACE
#C_FLAGS += -DWICED_BT_TRACE_CY_ENABLE


#uint8_t node_uuid_magic_bytes[4] = {0x41, 0x22, 0xB3, 0x44};
