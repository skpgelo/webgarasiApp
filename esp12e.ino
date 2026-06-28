#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

// Konfigurasi Wi-Fi & Firebase
#define WIFI_SSID "NAMA_WIFI_ANDA"
#define WIFI_PASSWORD "PASSWORD_WIFI_ANDA"
#define FIREBASE_HOST "PROJECT_://firebaseio.com"
#define FIREBASE_AUTH "DATABASE_SECRET_ANDA"

// Definisi Pin Input (Switch)
const int pinSW1 = D1;
 const int pinSW2 = D2;
 const int pinSW3 = D3;
 const int pinSW4 = D4;
 const int pinSW5 = D0;
 const int pinSW6 = D5; 

// Definisi Pin Output (Relay)
const int pinR1 = D6;
const int pinR2 = D7;
const int pinR3 = D8;
const int pinR4 = RX;
 const int pinOutCadangan = TX; 

// Variabel Status Relay
bool statusR1 = false;
bool statusR2 = false;
bool statusR3 = false;
bool statusR4 = false;

// Variabel Debounce Switch
bool lastStateSW1 = LOW;
bool lastStateSW2 = LOW;
bool lastStateSW3 = LOW;
bool lastStateSW4 = LOW;

unsigned long lastHeartbeat = 0;

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

void applyRelays() {
  digitalWrite(pinR1, statusR1 ? HIGH : LOW);
  digitalWrite(pinR2, statusR2 ? HIGH : LOW);
  digitalWrite(pinR3, statusR3 ? HIGH : LOW);
  digitalWrite(pinR4, statusR4 ? HIGH : LOW);
}

void updateFirebase() {
  Firebase.setInt(firebaseData, "/R1", statusR1 ? 1 : 0);
  Firebase.setInt(firebaseData, "/R2", statusR2 ? 1 : 0);
  Firebase.setInt(firebaseData, "/R3", statusR3 ? 1 : 0);
  Firebase.setInt(firebaseData, "/R4", statusR4 ? 1 : 0);
}

// FUNGSI BARU: Sinkronisasi data saat pertama kali alat menyala dari mati lampu
void syncFromFirebaseOnBoot() {
  if (Firebase.ready()) {
    if (Firebase.getInt(firebaseData, "/R1")) statusR1 = (firebaseData.intData() == 1);
    if (Firebase.getInt(firebaseData, "/R2")) statusR2 = (firebaseData.intData() == 1);
    if (Firebase.getInt(firebaseData, "/R3")) statusR3 = (firebaseData.intData() == 1);
    if (Firebase.getInt(firebaseData, "/R4")) statusR4 = (firebaseData.intData() == 1);
    applyRelays();
  }
}

void setup() {
  // Setup Pin Mode
  pinMode(pinSW1, INPUT_PULLDOWN);
  pinMode(pinSW2, INPUT_PULLDOWN);
  pinMode(pinSW3, INPUT_PULLDOWN);
  pinMode(pinSW4, INPUT_PULLDOWN);
  pinMode(pinSW5, INPUT_PULLDOWN);
  pinMode(pinSW6, INPUT_PULLDOWN);

  pinMode(pinR1, OUTPUT);
  pinMode(pinR2, OUTPUT);
  pinMode(pinR3, OUTPUT);
  pinMode(pinR4, OUTPUT);
  pinMode(pinOutCadangan, OUTPUT);

  // Jalankan kondisi mati default awal sebelum terhubung internet
  applyRelays();

  // Koneksi Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  // Koneksi Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Ambil data terakhir dari cloud agar relay kembali ke posisi sebelum mati lampu
  syncFromFirebaseOnBoot();
}

void loop() {
  // Membaca State Switch Saat Ini
  bool currentStateSW1 = digitalRead(pinSW1);
  bool currentStateSW2 = digitalRead(pinSW2);
  bool currentStateSW3 = digitalRead(pinSW3);
  bool currentStateSW4 = digitalRead(pinSW4);
  bool currentStateSW5 = digitalRead(pinSW5);
  bool currentStateSW6 = digitalRead(pinSW6);

  bool needUpdate = false;

  // Logika Switch 5 & 6 (Limit Reed Switch - Proteksi Utama)
  if (currentStateSW5 == HIGH || currentStateSW6 == HIGH) {
    if (statusR1 || statusR2) {
      statusR1 = false;
      statusR2 = false;
      needUpdate = true;
    }
  } else {
    // Logika Switch 1 (Toggle & Interlock dengan R2)
    if (currentStateSW1 == HIGH && lastStateSW1 == LOW) {
      if (!statusR1) {
        statusR1 = true;
        statusR2 = false;
      } else {
        statusR1 = false;
      }
      needUpdate = true;
      delay(200);
    }

    // Logika Switch 2 (Toggle & Interlock dengan R1)
    if (currentStateSW2 == HIGH && lastStateSW2 == LOW) {
      if (!statusR2) {
        statusR2 = true;
        statusR1 = false;
      } else {
        statusR2 = false;
      }
      needUpdate = true;
      delay(200);
    }
  }

  // Logika Switch 3 (Toggle Standalone)
  if (currentStateSW3 == HIGH && lastStateSW3 == LOW) {
    statusR3 = !statusR3;
    needUpdate = true;
    delay(200);
  }

  // Logika Switch 4 (Toggle Standalone)
  if (currentStateSW4 == HIGH && lastStateSW4 == LOW) {
    statusR4 = !statusR4;
    needUpdate = true;
    delay(200);
  }

  // Menyimpan state terakhir
  lastStateSW1 = currentStateSW1;
  lastStateSW2 = currentStateSW2;
  lastStateSW3 = currentStateSW3;
  lastStateSW4 = currentStateSW4;

  if (needUpdate) {
    applyRelays();
    updateFirebase();
  }

  // FITUR BARU: Mengirim sinyal detak jantung (Heartbeat) setiap 5 detik ke Firebase
  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();
    if (Firebase.ready()) {
      // Mengirim data timestamp berupa waktu milidetik perangkat berjalan
      Firebase.setInt(firebaseData, "/status/last_seen", millis());
    }
  }
}
