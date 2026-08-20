#pragma once

/* =======================================================================
 *  EDIT
 * =======================================================================
 */

/* --- WiFi SUMBER --- */
#define UPSTREAM_WIFI_SSID      "AYYUBI"
#define UPSTREAM_WIFI_PASS      "rumahabi123"

/* Isi channel WiFi router sumber kalau tau (1-13). Ini PENTING buat
 * ngurangin jitter: kalau diisi, ESP gak perlu scan semua channel pas
 * connect/reconnect, jadi radio gak "ngilang" lama dari AP saat itu
 * terjadi. Cek channel router di app router / WiFi analyzer.
 * 0 = auto (scan semua channel, reconnect lebih lama & lebih sering
 * bikin AP freeze sebentar). */
#define UPSTREAM_WIFI_CHANNEL   3

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

/* Batas berapa lama boleh nunggu sebelum retry connect ke upstream
 * kalau gagal terus-terusan (backoff), dalam ms. Biar gak spam
 * scan+connect yang bikin radio sibuk sendiri & AP ikut ke-freeze. */
#define STA_RECONNECT_MAX_DELAY_MS   8000
