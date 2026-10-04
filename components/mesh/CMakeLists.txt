# Actor Mesh (stub). core/ pure C, port/ interfaces, shell/ FreeRTOS task (AD-27).
# Requiring the Zigbee library links it into both firmwares (WHOLE_ARCHIVE),
# proving esp-zigbee-lib 2.0.4 builds on this ESP-IDF version.
idf_component_register(INCLUDE_DIRS "include"
                       PRIV_REQUIRES espressif__esp-zigbee-lib)
