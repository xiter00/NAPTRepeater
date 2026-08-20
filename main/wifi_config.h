#pragma once

/* =======================================================================
 *  EDIT
 * =======================================================================
 */

/* --- WiFi SUMBER --- */
#define UPSTREAM_WIFI_SSID      "namawifitarget"
#define UPSTREAM_WIFI_PASS      "passwordwifi"

/* --- WiFi ESP --- */
#define REPEATER_AP_SSID        "SERVERS3"
#define REPEATER_AP_PASS        "gaming12345"   /* min 8 karakter, WPA2 */
#define REPEATER_AP_CHANNEL     6               
#define REPEATER_AP_MAX_CONN    4                /* max device */
#define REPEATER_AP_HIDDEN      0                /* 0 = SSID visible, 1 = SSID hidden */

/* --- IP Address --- */
#define REPEATER_AP_IP          "192.168.4.1"
#define REPEATER_AP_GATEWAY     "192.168.4.1"
#define REPEATER_AP_NETMASK     "255.255.255.0"

/* --- Tuning latency (jangan diubah kalo gak paham) --- */
#define WIFI_TX_POWER_DBM       80    
#define USE_HT40_BANDWIDTH      0     
