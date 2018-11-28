# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/esp-mqtt $(IDF_PATH)/components/esp-mqtt/lwmqtt/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp-mqtt -lesp-mqtt
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp-mqtt
COMPONENT_LDFRAGMENTS += 
component-esp-mqtt-build: 
