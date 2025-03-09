/* Water Level Monitoring System with Blynk, DHT11 & Ultrasonic Sensor */

// ========== Blynk Configuration ==========
#define BLYNK_TEMPLATE_ID "TMPL6V_OCox4J"
#define BLYNK_TEMPLATE_NAME "Flood Detector"
#define BLYNK_AUTH_TOKEN "6XKbNtbolmp-nf66mvdxIST89tOHl_U7"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <algorithm> // สำหรับใช้ std::sort

// ========== Wi-Fi Credentials ==========
const char *ssid = "Cosmo";         // 🔴 ใส่ชื่อ Wi-Fi
const char *password = "123456789"; // 🔴 ใส่รหัสผ่าน Wi-Fi

BlynkTimer timer;

// ========== Ultrasonic Sensor Pins ==========
#define trig D7 // GPIO13
#define echo D8 // GPIO15

// ========== DHT11 Sensor ==========
#define DHTPIN D4 // GPIO2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ========== LED Alert Pin ==========
#define ledPin D2 // GPIO4

// ========== Variables ==========
bool highWaterNotified = false;
bool alertsEnabled = true;

// Last recorded values
int lastWaterLevel = -1;
float lastTemp = 0.0, lastHum = 0.0;

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup()
{
  Serial.begin(9600);

  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password, "blynk.cloud", 80);

  // ตั้งให้ทำงานทุก 1 วินาที (วัดระดับน้ำ) และ 2 วินาที (วัดอุณหภูมิ/ความชื้น)
  timer.setInterval(2000L, ultrasonic);
  timer.setInterval(2000L, readDHT);
}

// ----------------------------------------------------------------------------
// READ DHT SENSOR (Humidity & Temperature)
// ----------------------------------------------------------------------------
void readDHT()
{
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("⚠️ ERROR: Failed to read from DHT sensor! Using last known values.");
    temperature = lastTemp;
    humidity = lastHum;
  }
  else
  {
    lastTemp = temperature;
    lastHum = humidity;
  }

  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, humidity);

  Serial.print("🌡 Temp: ");
  Serial.print(temperature);
  Serial.print("°C | 💧 Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
}

// ----------------------------------------------------------------------------
// READ ULTRASONIC SENSOR (Median Filter)
// ----------------------------------------------------------------------------
long readUltrasonic()
{
  const int samples = 5;
  long durations[samples];

  for (int i = 0; i < samples; i++)
  {
    digitalWrite(trig, LOW);
    delayMicroseconds(2);
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);

    long duration = pulseIn(echo, HIGH, 30000);
    if (duration > 0 && duration < 30000)
    {
      durations[i] = duration;
    }
    else
    {
      i--; // อ่านใหม่ถ้าค่าผิดปกติ
    }
    delay(10);
  }

  // ใช้วิธีการหา Median เพื่อลด Noise
  std::sort(durations, durations + samples);
  return durations[samples / 2];
}

// ----------------------------------------------------------------------------
// ULTRASONIC: Measure Water Level & Trigger Alerts
// ----------------------------------------------------------------------------
void ultrasonic()
{
  long duration = readUltrasonic();
  int distance = duration * 0.034 / 2;
  int waterLevelPercent = map(distance, 0, 100, 100, 0);
  waterLevelPercent = constrain(waterLevelPercent, 0, 100);

  String levelLabel = (waterLevelPercent >= 75) ? "High" : (waterLevelPercent >= 35) ? "Medium"
                                                                                     : "Low";

  Blynk.virtualWrite(V0, waterLevelPercent);
  Blynk.virtualWrite(V3, levelLabel);

  Serial.print("Water Level: ");
  Serial.print(waterLevelPercent);
  Serial.print("%, Status: ");
  Serial.println(levelLabel);

  if (waterLevelPercent != lastWaterLevel)
  {
    lastWaterLevel = waterLevelPercent;

    if (alertsEnabled)
    {
      if (waterLevelPercent >= 75 && !highWaterNotified)
      {
        digitalWrite(ledPin, HIGH);
        Blynk.virtualWrite(V5, 1);
        highWaterNotified = true;
      }
      else if (waterLevelPercent < 75 && highWaterNotified)
      { // ต้องต่ำกว่า 75% ก่อนปิด
        digitalWrite(ledPin, LOW);
        Blynk.virtualWrite(V5, 0);
        highWaterNotified = false;
      }
    }
    else
    {
      digitalWrite(ledPin, LOW);
      Blynk.virtualWrite(V5, 0);
      highWaterNotified = false;
    }
  }
}

// ----------------------------------------------------------------------------
// LOOP
// ----------------------------------------------------------------------------
void loop()
{
  Blynk.run();
  timer.run();
}
