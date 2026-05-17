//INITIALIZATION
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

//WIFI SETUP
const char* ssid = "gi";
const char* password = "giiooo0301";

//THINGSBOARD SETUP
const char* mqtt_server = "mqtt.thingsboard.cloud";
const char* token = "Awye4h7A6HJlh3rebZw7";

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
#define RAIN_PIN 13

//TINGGI TANGKI
float tinggiTangki = 100.0; // cm

//STATUS AWAL NOTIFIKASI
bool warningTerkirim = false;

// WIFI
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

// MQTT RECONNECT
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

// BACA ULTRASONIK
float bacaUltrasonik() {
 digitalWrite(TRIG_PIN, LOW);
 delayMicroseconds(2);
 digitalWrite(TRIG_PIN, HIGH);
 delayMicroseconds(10);
 digitalWrite(TRIG_PIN, LOW);
 long duration = pulseIn(ECHO_PIN, HIGH);
 float distance = duration * 0.034 / 2;
 return distance;
}

// SETUP
void setup() {
 Serial.begin(115200);
 pinMode(TRIG_PIN, OUTPUT);
 pinMode(ECHO_PIN, INPUT);
 setupWifi();
 secured_client.setInsecure();
 client.setServer(mqtt_server, 1883);
 Serial.println("System Ready");
}
// LOOP SYSTEM
void loop() {
 //MQTT
 if (!client.connected()) {
   reconnect();
 }
 client.loop();
 //SENSOR ULTRASONIK
 float jarak = bacaUltrasonik();
 float tinggiAir = tinggiTangki - jarak;   //menghitung tinggi air
 if (tinggiAir < 0) {
   tinggiAir = 0;
 }
 //RAIN SENSOR
 int rainValue = analogRead(RAIN_PIN);
 bool hujan = false; //semakin kecil nilainya -> semakin basah
 if (rainValue < 2000) {
   hujan = true;
 }
 //SERIAL MONITOR
 Serial.println("======================");
 Serial.print("Jarak Air: ");
 Serial.print(jarak);
 Serial.println(" cm");
 Serial.print("Tinggi Air: ");
 Serial.print(tinggiAir);
 Serial.println(" cm");
 Serial.print("Rain Value: ");
 Serial.println(rainValue);
 Serial.print("Hujan: ");
 if (hujan) {
   Serial.println("YA");
 } else {
   Serial.println("TIDAK");
 }
 //KIRIM TELEMETRY
 String payload = "{";
 payload += "\"water_level\":";
 payload += tinggiAir;
 payload += ",";
 payload += "\"rain_value\":";
 payload += rainValue;
 payload += ",";
 payload += "\"is_raining\":";
 payload += (hujan ? "true" : "false");
 payload += "}";
 Serial.println(payload);
 client.publish("v1/devices/me/telemetry", payload.c_str());
 //TELEGRAM ALERT (alert akan menyala jika air lebih dari 80 cm)
 if (tinggiAir > 80 && !warningTerkirim) {
   String pesan = "⚠️ WARNING BANJIR ⚠️\n\n";
   pesan += "Ketinggian air: ";
   pesan += String(tinggiAir);
   pesan += " cm\n";
   if (hujan) {
     pesan += "Status Hujan: YA";
   } else {
     pesan += "Status Hujan: TIDAK";
   }
   bot.sendMessage(CHAT_ID, pesan, "");
   Serial.println("Notifikasi Telegram terkirim");
   warningTerkirim = true;
 }
 //RESET ALERT (kalau air turun lagi)
 if (tinggiAir < 70) {
   warningTerkirim = false;
 }
 delay(5000);
}

