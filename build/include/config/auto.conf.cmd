deps_config := \
	/home/rutsch/esp/esp-idf/components/app_trace/Kconfig \
	/home/rutsch/esp/esp-idf/components/aws_iot/Kconfig \
	/home/rutsch/esp/esp-idf/components/bt/Kconfig \
	/home/rutsch/esp/esp-idf/components/driver/Kconfig \
	/home/rutsch/esp/esp-idf/components/esp-mqtt/Kconfig \
	/home/rutsch/esp/esp-idf/components/esp32/Kconfig \
	/home/rutsch/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/rutsch/esp/esp-idf/components/esp_event/Kconfig \
	/home/rutsch/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/rutsch/esp/esp-idf/components/esp_http_server/Kconfig \
	/home/rutsch/esp/esp-idf/components/ethernet/Kconfig \
	/home/rutsch/esp/esp-idf/components/fatfs/Kconfig \
	/home/rutsch/esp/esp-idf/components/freemodbus/Kconfig \
	/home/rutsch/esp/esp-idf/components/freertos/Kconfig \
	/home/rutsch/esp/esp-idf/components/heap/Kconfig \
	/home/rutsch/esp/esp-idf/components/libsodium/Kconfig \
	/home/rutsch/esp/esp-idf/components/log/Kconfig \
	/home/rutsch/esp/esp-idf/components/lwip/Kconfig \
	/home/rutsch/esp/esp-idf/components/mbedtls/Kconfig \
	/home/rutsch/esp/esp-idf/components/mdns/Kconfig \
	/home/rutsch/esp/esp-idf/components/mqtt/Kconfig \
	/home/rutsch/esp/esp-idf/components/nvs_flash/Kconfig \
	/home/rutsch/esp/esp-idf/components/openssl/Kconfig \
	/home/rutsch/esp/esp-idf/components/pthread/Kconfig \
	/home/rutsch/esp/esp-idf/components/spi_flash/Kconfig \
	/home/rutsch/esp/esp-idf/components/spiffs/Kconfig \
	/home/rutsch/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/rutsch/esp/esp-idf/components/unity/Kconfig \
	/home/rutsch/esp/esp-idf/components/vfs/Kconfig \
	/home/rutsch/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/rutsch/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/rutsch/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/rutsch/esp/cactus/main/Kconfig.projbuild \
	/home/rutsch/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/rutsch/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_TARGET)" "esp32"
include/config/auto.conf: FORCE
endif
ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
