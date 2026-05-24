//INITIALIZATION
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

//WIFI SETUP
const char* ssid = "nyala";
const char* password = "nyala1234";

//THINGSBOARD SETUP
const char* mqtt_server = "mqtt.thingsboard.cloud";
const char* token = "1LqZFLYjvJHYSpadXSg3";

//TELEGRAM SETUP
#define BOT_TOKEN "8708131739:AAEf5GgNbG3aNdZL7KrkQmGbAnqsOTuxZ1E"
#define CHAT_ID "1191164119"

WiFiClient espClient;
PubSubClient client(espClient);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

//PIN SENSOR
#define TRIG_PIN 5
#define ECHO_PIN 18

// WATER LEVEL SENSOR
const int WATER_LEVEL_PIN = 13;
const int WATER_POWER_PIN = 4;

//TINGGI TANGKI
float tinggiTangki = 28.60; // cm

//STATUS AWAL NOTIFIKASI
bool warningTerkirim = false;

//VARIABEL ULTRASONIK
float jarak, tinggiAir;

// VARIABEL CURAH HUJAN
unsigned long waktuLama = 0;
const long intervalWaktu = 60000; // Cek kenaikan air setiap 1 Menit

int nilaiAwal = 0;
int nilaiAkhir = 0;
int kenaikanAir = 0;

String kategoriHujan = "Belum Ada Data";

// WIFI SETUP FUNCTION
void setupWifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
}

// MQTT RECONNECT FUNCTION
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard...");
    if (client.connect("ESP32_Client", token, NULL)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 2 seconds");
      delay(2000);
    }
  }
}

// BACA ULTRASONIK (Mengambil rata-rata 5x pembacaan agar stabil)
float bacaUltrasonik() {
  float total = 0;
  for (int i = 0; i < 5; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    float distance = duration * 0.034 / 2;
    total += distance;
    delay(30);
  }
  return total / 5.0;
}

// ARDUINO SETUP
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Setup Water Level Sensor
  pinMode(WATER_POWER_PIN, OUTPUT);
  digitalWrite(WATER_POWER_PIN, LOW);

  // Ambil data awal saat alat pertama kali dinyalakan
  digitalWrite(WATER_POWER_PIN, HIGH);
  delay(20);
  nilaiAwal = analogRead(WATER_LEVEL_PIN);
  digitalWrite(WATER_POWER_PIN, LOW);

  waktuLama = millis();

  setupWifi();
  secured_client.setInsecure();
  client.setServer(mqtt_server, 1883);

  Serial.println("Sistem Pemantau Curah Hujan Aktif...");
  Serial.println("System Ready");
}

// MAIN LOOP
void loop() {
  // Pastikan MQTT Terkoneksi
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 1. PROSES SENSOR ULTRASONIK
  jarak = bacaUltrasonik();
  tinggiAir = tinggiTangki - jarak;

  // Proteksi nilai minus
  if (tinggiAir < 0) {
    tinggiAir = 0;
  }

  // 2. PROSES CURAH HUJAN (Setiap 1 menit)
  unsigned long waktuSekarang = millis();

  if (waktuSekarang - waktuLama >= intervalWaktu) {
    // Baca nilai akhir setelah 1 menit terisi air hujan
    digitalWrite(WATER_POWER_PIN, HIGH);
    delay(20);
    nilaiAkhir = analogRead(WATER_LEVEL_PIN);
    digitalWrite(WATER_POWER_PIN, LOW);

    // Hitung seberapa banyak air bertambah
    kenaikanAir = nilaiAkhir - nilaiAwal;

    if (kenaikanAir < 0) {
      kenaikanAir = 0;
    }

    // Tentukan Kategori Curah Hujan
    if (kenaikanAir < 15) {
      kategoriHujan = "Tidak Ada Hujan / Berawan";
    } 
    else if (kenaikanAir >= 15 && kenaikanAir < 200) {
      kategoriHujan = "Curah Hujan Rendah (Gerimis)";
    } 
    else if (kenaikanAir >= 200 && kenaikanAir < 600) {
      kategoriHujan = "Curah Hujan Sedang";
    } 
    else {
      kategoriHujan = "Curah Hujan Tinggi (Lebat / Deras!)";
    }

    // Serial Monitor khusus Curah Hujan tiap menit
    Serial.println("======================");
    Serial.print("[DATA CURAH HUJAN] Kenaikan Nilai: ");
    Serial.print(kenaikanAir);
    Serial.print(" | Kategori: ");
    Serial.println(kategoriHujan);

    // Reset nilaiAwal & perbarui waktu
    nilaiAwal = nilaiAkhir;
    waktuLama = waktuSekarang;
  }

  // 3. SERIAL MONITOR UTAMA (Tampil setiap loop)
  Serial.println("======================");
  Serial.print("Jarak Air: ");
  Serial.print(jarak);
  Serial.println(" cm");
  Serial.print("Tinggi Air: ");
  Serial.print(tinggiAir);
  Serial.println(" cm");
  Serial.print("Kenaikan Air Hujan: ");
  Serial.println(kenaikanAir);
  Serial.print("Kategori Curah Hujan: ");
  Serial.println(kategoriHujan);

  // 4. KIRIM TELEMETRY KE THINGSBOARD
  String payload = "{";
  payload += "\"water_level\":";
  payload += tinggiAir;
  payload += ",";
  payload += "\"rain_increase\":";
  payload += kenaikanAir;
  payload += ",";
  payload += "\"rain_category\":\"";
  payload += kategoriHujan;
  payload += "\"";
  payload += "}";

  Serial.println(payload);
  client.publish("v1/devices/me/telemetry", payload.c_str());

  // 5. TELEGRAM ALERT (Menyala jika air > 16.50 cm)
  if (tinggiAir > 16.50 && !warningTerkirim) {
    String pesan = "⚠️ WARNING BANJIR ⚠️\n\n";
    pesan += "Ketinggian air: ";
    pesan += String(tinggiAir);
    pesan += " cm\n";
    pesan += "Kategori Curah Hujan: ";
    pesan += kategoriHujan;

    bot.sendMessage(CHAT_ID, pesan, "");
    Serial.println("Notifikasi Telegram terkirim");
    warningTerkirim = true;
  }

  // RESET ALERT (Jika air turun di bawah 6.00 cm)
  if (tinggiAir < 6.00) {
    warningTerkirim = false;
  }

  delay(5000);
}
