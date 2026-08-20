#include <WiFi.h>
#include <lwip/napt.h>
#include <lwip/err.h>

// --- GANTI BAGIAN INI SESUAI WIFI LU ---
const char* ssid_sumber = "AYYUBI"; 
const char* password_sumber = "rumahabi123";

const char* ssid_repeater = "SERVERS3"; // Nama Wi-Fi buatan ESP32
const char* password_repeater = "gasmain123";    // Minimal 8 karakter
// ---------------------------------------

// Alokasi memori buat tabel NAT
#define NAPT_TAB_SIZE 1000 
#define NAPT_PORT_TAB_SIZE 10

void setup() {
  Serial.begin(115200);
  Serial.println("\n[+] Memulai ESP32-S3 NAT Router...");

  // Set mode jadi Penerima (STA) & Pemancar (AP) sekaligus
  WiFi.mode(WIFI_AP_STA);

  // 1. Konek ke Wi-Fi Utama
  WiFi.begin(ssid_sumber, password_sumber);
  Serial.print("[+] Konek ke ");
  Serial.print(ssid_sumber);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[+] Konek! IP dari Router: " + WiFi.localIP().toString());

  // 2. Bikin Wi-Fi Baru buat HP lu
  WiFi.softAP(ssid_repeater, password_repeater);
  Serial.println("[+] Wi-Fi Repeater Aktif! IP: " + WiFi.softAPIP().toString());

  // 3. Aktifin NAT (Jantungnya Repeater)
  err_t ret = ip_napt_init(NAPT_TAB_SIZE, NAPT_PORT_TAB_SIZE);
  if (ret == ERR_OK) {
    // Enable NAT di interface SoftAP
    ret = ip_napt_enable_no(WIFI_IF_AP, 1); 
    if (ret == ERR_OK) {
      Serial.println("[+] NAT Berhasil Aktif! Sistem stabil, siap disiksa main RDR2.");
    }
  }
  
  if (ret != ERR_OK) {
    Serial.println("[-] Gagal mengaktifkan NAT. Cek versi Arduino Core lu.");
  }
}

void loop() {
  // Biarin kosong. Proses NAT berjalan otomatis di background 
  // menggunakan core lain berkat FreeRTOS & LwIP bawaan ESP32.
  delay(1000); 
}
