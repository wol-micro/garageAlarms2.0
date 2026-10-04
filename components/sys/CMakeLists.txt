# Actor Sys (stub) plus the boot banner shared by both apps.
# core/ pure C, port/ interfaces, shell/ FreeRTOS task (AD-27).
idf_component_register(SRCS "shell/sys_banner.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ga_config esp_app_format esp_hw_support log)
