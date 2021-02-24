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
C_FLAGS += -DREMOTE_PROVISION_SERVER_SUPPORTED
C_FLAGS += -DMESH_DFU_SUPPORTED
# C_FLAGS += -DREMOTE_PROVISION_OVER_GATT_SUPPORTED
# C_FLAGS += -DGATT_PROXY_CLIENT_SUPPORTED
C_FLAGS += -DUSE_VS_NVRAM
C_FLAGS += -DUSE_VENDOR_UUID

########################################################################
# Mesh models enabled in mesh_provision_client. All client apps may not
# fit on all platforms. Add Application sources required for the client here.
########################################################################
APP_SRC  = mesh_provisioner_node.c
APP_SRC += wiced_mesh_db.c
APP_SRC += wiced_mesh_client.c

APP_SRC += mesh_btn_handler.c

#APP_SRC += mesh_default_transition_time_client.c
APP_SRC += mesh_vendor_specific_app.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_ONOFF_CLIENT_INCLUDED
#APP_SRC += mesh_onoff_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_LEVEL_CLIENT_INCLUDED
#APP_SRC += mesh_level_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_LIGHT_LIGHTNESS_CLIENT_INCLUDED
#APP_SRC += mesh_light_lightness_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_LIGHT_HSL_CLIENT_INCLUDED
#APP_SRC += mesh_light_hsl_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_LIGHT_CTL_CLIENT_INCLUDED
#APP_SRC += mesh_light_ctl_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_SENSOR_CLIENT_INCLUDED
#APP_SRC += mesh_sensor_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_SCENE_CLIENT_INCLUDED
#APP_SRC += mesh_scene_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_PROPERTY_CLIENT_INCLUDED
#APP_SRC += mesh_property_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_LIGHT_LC_CLIENT_INCLUDED
#APP_SRC += mesh_light_lc_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_BATTERY_CLIENT_INCLUDED
#APP_SRC += mesh_battery_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_LIGHT_XYL_CLIENT_INCLUDED
#APP_SRC += mesh_light_xyl_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_LOCATION_CLIENT_INCLUDED
#APP_SRC += mesh_location_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_POWER_LEVEL_CLIENT_INCLUDED
#APP_SRC += mesh_power_level_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_POWER_ONOFF_CLIENT_INCLUDED
#APP_SRC += mesh_power_onoff_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_SCHEDULER_CLIENT_INCLUDED
#APP_SRC += mesh_scheduler_client.c

#C_FLAGS += -DWICED_BT_MESH_MODEL_TIME_CLIENT_INCLUDED
#APP_SRC += mesh_time_client.c

########################################################################
# Component(s) needed
########################################################################
$(NAME)_COMPONENTS := mesh_app_lib.a
$(NAME)_COMPONENTS += fw_upgrade_lib.a
$(NAME)_COMPONENTS += hal_vs_nvram_lib.a
$(NAME)_COMPONENTS += gatt_utils_lib.a

ifeq ($(MESH_MODELS_DEBUG),1)
$(NAME)_COMPONENTS += mesh_models_lib-d.a
else
$(NAME)_COMPONENTS += mesh_models_lib.a
endif
ifeq ($(MESH_CORE_DEBUG),1)
$(NAME)_COMPONENTS += mesh_core_lib-d.a
else
$(NAME)_COMPONENTS += mesh_core_lib.a
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

########################################################################
# C flags
########################################################################
C_FLAGS += -DMESH_PROVISION_CLIENT

# define this for 2 chip solution or for testing over WICED HCI
# C_FLAGS += -DHCI_CONTROL
C_FLAGS += -DTRANSPORT_BUFFER_SIZE=400
#C_FLAGS += -D_DEB_ENABLE_HCI_TRACE
C_FLAGS += -DWICED_BT_TRACE_ENABLE
