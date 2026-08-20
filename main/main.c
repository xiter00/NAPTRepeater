/*
 * ESP32-S3 (N16R8) WiFi Repeater / NAT Router
 * Dioptimasi untuk latency rendah (cloud gaming: GeForce NOW, Xbox Cloud, dsb)
 *
 * Mode kerja: AP + STA (single radio 2.4GHz -> AP & STA otomatis 1 channel)
 * Routing   : lwIP NAPT (Network Address & Port Translation) -> full router,
 *             bukan cuma "extender" biasa, sehingga jauh lebih stabil.
 *
 * Edit kredensial WiFi di file wifi_config.h SEBELUM compile.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "lwip/lwip_napt.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "esp_task_wdt.h"

#include "wifi_config.h"

static const char *TAG = "wifi_repeater";

static EventGroupHandle_t s_wifi_event_group;
#define STA_CONNECTED_BIT   BIT0
#define STA_FAIL_BIT        BIT1

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

static int s_retry_num = 0;
#define MAX_RETRY_IMMEDIATE  5

/* -------------------------------------------------------------------- */
/* tuning latency & stabilizer      */
/* -------------------------------------------------------------------- */
static void apply_low_latency_wifi_tuning(void)
{

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));


    wifi_bandwidth_t bw = USE_HT40_BANDWIDTH ? WIFI_BW_HT40 : WIFI_BW_HT20;
    esp_wifi_set_bandwidth(WIFI_IF_AP, bw);
    esp_wifi_set_bandwidth(WIFI_IF_STA, bw);


    esp_wifi_set_max_tx_power(WIFI_TX_POWER_DBM);

    
    esp_wifi_set_protocol(WIFI_IF_STA,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    esp_wifi_set_protocol(WIFI_IF_AP,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
}

/* -------------------------------------------------------------------- */
/* Event handler WiFi & IP                                              */
/* -------------------------------------------------------------------- */
static void event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Upstream WiFi terputus, mencoba reconnect...");
        xEventGroupClearBits(s_wifi_event_group, STA_CONNECTED_BIT);

        /* JANGAN vTaskDelay() di sini -> ini system event task, dipakai
         * buat proses SEMUA event WiFi/IP. Kalau di-block, seluruh
         * sistem (termasuk jalur NAPT) ikut freeze selama itu juga.
         * Reconnect langsung tanpa nunggu -> freeze yang kerasa pas
         * kontrol dimainin ilang, sisanya (watchdog_task) tetap jadi
         * jaring pengaman kalau reconnect langsung ini gagal. */
        esp_wifi_connect();
        s_retry_num++;

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Terhubung ke upstream WiFi, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, STA_CONNECTED_BIT);


        esp_netif_napt_enable(ap_netif);
        ESP_LOGI(TAG, "NAT/NAPT aktif. Client di AP sekarang bisa akses internet.");

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "Device baru konek ke repeater, MAC: " MACSTR, MAC2STR(e->mac));

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "Device putus dari repeater, MAC: " MACSTR, MAC2STR(e->mac));
    }
}


static void wifi_init_ap(void)
{
    ap_netif = esp_netif_create_default_wifi_ap();


    esp_netif_dhcps_stop(ap_netif);
    esp_netif_ip_info_t ip_info;
    ip4addr_aton(REPEATER_AP_IP, (ip4_addr_t *)&ip_info.ip);
    ip4addr_aton(REPEATER_AP_GATEWAY, (ip4_addr_t *)&ip_info.gw);
    ip4addr_aton(REPEATER_AP_NETMASK, (ip4_addr_t *)&ip_info.netmask);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dhcps_start(ap_netif);

    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = strlen(REPEATER_AP_SSID),
            .channel = REPEATER_AP_CHANNEL,
            .max_connection = REPEATER_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = REPEATER_AP_HIDDEN,
            .beacon_interval = 100,
        },
    };
    strncpy((char *)ap_config.ap.ssid, REPEATER_AP_SSID, sizeof(ap_config.ap.ssid));
    strncpy((char *)ap_config.ap.password, REPEATER_AP_PASS, sizeof(ap_config.ap.password));

    if (strlen(REPEATER_AP_PASS) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
}


static void wifi_init_sta(void)
{
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_config_t sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .scan_method = WIFI_FAST_SCAN,         
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            /* rm/btm (802.11k/v) dimatikan: kalau nyala, STA kadang harus
             * "nengok" ke channel lain buat ngirim neighbor report ke
             * router utama. Karena radio AP+STA di sini cuma satu, pas
             * nengok itu koneksi ke client freeze sepersekian detik ->
             * kerasa jadi jitter pas kontrol dimainin (video gak begitu
             * kerasa karena kebantu buffer). */
            .rm_enabled = 0,
            .btm_enabled = 0,
            .listen_interval = 3,                    
        },
    };
    strncpy((char *)sta_config.sta.ssid, UPSTREAM_WIFI_SSID, sizeof(sta_config.sta.ssid));
    strncpy((char *)sta_config.sta.password, UPSTREAM_WIFI_PASS, sizeof(sta_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
}


static void watchdog_task(void *pvParameters)
{
    while (1) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        if (!(bits & STA_CONNECTED_BIT)) {
            ESP_LOGW(TAG, "STA belum terhubung, retry connect...");
            esp_wifi_connect();
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3 N16R8 WiFi Repeater (low-latency) booting ===");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* Perbesar buffer RX/TX untuk mengurangi packet drop saat lalu-lintas
     * padat (banyak device / streaming game) -> mengurangi jitter */
    cfg.static_rx_buf_num = 16;
    cfg.dynamic_rx_buf_num = 32;
    cfg.dynamic_tx_buf_num = 32;
    /* AMPDU RX & TX tetap NYALA (jangan dimatiin total, itu bikin downlink
     * video/stream jadi boros airtime -> radio penuh -> ping malah drop).
     * Fix jitter kontrol dilakukan lewat BA window yang diperkecil di
     * sdkconfig.defaults (CONFIG_ESP_WIFI_TX_BA_WIN), bukan di sini. */
    cfg.ampdu_rx_enable = 1;
    cfg.ampdu_tx_enable = 1;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_init_ap();
    wifi_init_sta();

    ESP_ERROR_CHECK(esp_wifi_start());

    apply_low_latency_wifi_tuning();

    ESP_LOGI(TAG, "AP aktif -> SSID: %s", REPEATER_AP_SSID);
    ESP_LOGI(TAG, "Menghubungkan ke upstream WiFi -> SSID: %s", UPSTREAM_WIFI_SSID);

    xTaskCreate(watchdog_task, "wifi_watchdog", 4096, NULL, 5, NULL);
}
