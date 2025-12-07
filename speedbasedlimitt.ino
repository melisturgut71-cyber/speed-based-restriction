#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <PubSubClient.h>

// --- AYARLAR (Lütfen Doldurun) ---
const char* ssid = "YourWİFİ";       
const char* password = "Yourpassword";         
const char* mqtt_server = "broker.hivemq.com"; 

// Joystick Ayarları
#define JOY_PIN 34
const int JOY_DEADZONE = 2000; // Orta nokta (yaklaşık)
float sanalHiz = 0; // Aracın simüle edilen hızı

// Nesne Tanımlamaları
TinyGPSPlus gps;
HardwareSerial gpsSerial(2); // RX=16, TX=17
WiFiClient espClient;
PubSubClient client(espClient);

// Global Değişkenler (Yol verilerini saklamak için)
String globalYolAdi = "Veri Bekleniyor...";
String globalYolTipi = "-";
int globalHizLimiti = 50;
String globalKaynak = "-";

unsigned long lastApiCall = 0;
unsigned long apiInterval = 15000; // 15 saniyede bir internete sor
unsigned long lastMqttSend = 0;

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(JOY_PIN, INPUT);

  // Wi-Fi Bağlantısı
  WiFi.begin(ssid, password);
  Serial.print("Wi-Fi Bağlanıyor");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Bağlandı!");

  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32Hybrid-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT Bağlandı");
    } else {
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // 1. GPS OKUMA
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  // 2. JOYSTICK İLE HIZ KONTROLÜ (Simülasyon)
  int joyVal = analogRead(JOY_PIN);
  
  // Gaz (İleri itince değer artıyorsa 4095'e, azalıyorsa 0'a göre ayarlayın)
  // ESP32'de genelde orta nokta ~1800-2000 arasıdır.
  if (joyVal > 2200) { 
    sanalHiz += 0.5; // Hızlanma katsayısı
  } 
  // Fren (Geri çekme)
  else if (joyVal < 1500) { 
    sanalHiz -= 1.0; // Frenleme katsayısı
  }
  else {
    sanalHiz -= 0.1; // Sürtünme (Kendi kendine yavaşlama)
  }

  // Hız Sınırları (0 ile 200 arası)
  if (sanalHiz < 0) sanalHiz = 0;
  if (sanalHiz > 220) sanalHiz = 220;


  // 3. API SORGUSU (Her 15 Saniyede Bir)
  if ((millis() - lastApiCall) > apiInterval) {
    if (gps.location.isValid()) {
      getYolBilgisi(gps.location.lat(), gps.location.lng());
    }
    lastApiCall = millis();
  }

  // 4. MQTT VERİ GÖNDERİMİ (Saniyede 4 kere - Akıcı olması için)
  if (millis() - lastMqttSend > 250) {
    sendMqttData();
    lastMqttSend = millis();
  }
}

// --- YARDIMCI FONKSİYONLAR ---

void sendMqttData() {
  DynamicJsonDocument doc(1024);
  
  // Simülasyon Verisi
  doc["hiz"] = (int)sanalHiz; 
  
  // GPS Verisi
  if (gps.location.isValid()) {
    doc["lat"] = gps.location.lat();
    doc["lon"] = gps.location.lng();
  } else {
    doc["lat"] = 0; doc["lon"] = 0;
  }

  // API'den Gelen Veriler (Global değişkenlerden al)
  doc["yol"] = globalYolAdi;
  doc["tip"] = globalYolTipi;
  doc["limit"] = globalHizLimiti;
  doc["kaynak"] = globalKaynak;

  // UYARI MANTIĞI: Sanal Hız > Gerçek Limit mi?
  if (sanalHiz > globalHizLimiti) {
    doc["uyari"] = true;
  } else {
    doc["uyari"] = false;
  }

  String output;
  serializeJson(doc, output);
  client.publish("arac/takip/hibrit", output.c_str());
  
  // Debug için sadece hızı yazalım
  // Serial.println("Hız: " + String(sanalHiz) + " / Limit: " + String(globalHizLimiti));
}

void getYolBilgisi(float lat, float lon) {
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure(); 

    String query = "%5Bout:json%5D;way(around:25," + String(lat, 6) + "," + String(lon, 6) + ")%5Bhighway%5D;out%20tags;";
    String serverPath = "https://overpass-api.de/api/interpreter?data=" + query;

    http.begin(httpsClient, serverPath);
    http.addHeader("User-Agent", "ESP32_Tez_Projesi"); 
    
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument apiDoc(4096); 
      deserializeJson(apiDoc, payload);
      
      JsonArray elements = apiDoc["elements"];
      if (elements.size() > 0) {
        JsonObject tags = elements[0]["tags"];
        
        // Verileri Global Değişkenlere Ata
        if (tags.containsKey("name")) globalYolAdi = tags["name"].as<String>();
        else globalYolAdi = "Bilinmeyen Yol";

        String tempTip = tags["highway"].as<String>();
        
        // Hız ve Tip Analizi
        if (tags.containsKey("maxspeed")) {
           globalHizLimiti = tags["maxspeed"].as<int>();
           globalKaynak = "Tabela";
        } else {
           globalKaynak = "Tahmin";
           if (tempTip == "motorway") { globalYolTipi = "OTOBAN"; globalHizLimiti = 130; }
           else if (tempTip == "primary") { globalYolTipi = "ANA YOL"; globalHizLimiti = 90; }
           else if (tempTip == "residential") { globalYolTipi = "MAHALLE"; globalHizLimiti = 50; }
           else { globalYolTipi = "YOL"; globalHizLimiti = 50; }
        }
      }
    }
    http.end();
  }
}
