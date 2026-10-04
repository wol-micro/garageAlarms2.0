#include "sys.h"

#include <inttypes.h>
#include <stdint.h>

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "ga_config.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef CONFIG_LWIP_TCPIP_TASK_PRIO
/* The task map (ga_config.h) keeps every actor below lwIP; its copy of the
 * lwIP priority must match the real one. */
GA_STATIC_ASSERT(GA_TASK_LWIP_PRIO == CONFIG_LWIP_TCPIP_TASK_PRIO,
                 "GA_TASK_LWIP_PRIO differs from CONFIG_LWIP_TCPIP_TASK_PRIO");
#endif

static const char *TAG = "sys";

/* EUI-64 is 8 bytes by definition (IEEE 802.15.4); not a tunable. */
#define SYS_EUI64_LEN 8
/* Base MAC is 6 bytes (EUI-48); not a tunable. */
#define SYS_MAC48_LEN 6

void sys_log_banner(const char *role)
{
    uint8_t eui[SYS_EUI64_LEN] = {0};
    const char *marker = "";
    esp_err_t err;

#if SOC_IEEE802154_SUPPORTED
    /* Own 802.15.4 EUI-64 (ESP32-H2 collector). */
    err = esp_read_mac(eui, ESP_MAC_IEEE802154);
#else
    /* No 802.15.4 radio here (ESP32-S3 logic node): build a provisional EUI-64
     * from the base MAC by inserting FFFE in the middle. Log value only, never
     * stored, sent or used as a node ID; story 1.3 logs the RCP's real EUI-64. */
    uint8_t mac[SYS_MAC48_LEN] = {0};
    err = esp_read_mac(mac, ESP_MAC_BASE);
    eui[0] = mac[0];
    eui[1] = mac[1];
    eui[2] = mac[2];
    eui[3] = 0xFF;
    eui[4] = 0xFE;
    eui[5] = mac[3];
    eui[6] = mac[4];
    eui[7] = mac[5];
    marker = "(provisional)";
#endif
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_read_mac failed: %s", esp_err_to_name(err));
    }

    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG,
             "role=%s eui64=%02X%02X%02X%02X%02X%02X%02X%02X%s fw=%s ga_config=%d",
             role, eui[0], eui[1], eui[2], eui[3], eui[4], eui[5], eui[6], eui[7],
             marker, app->version, GA_CONFIG_VERSION);
}
