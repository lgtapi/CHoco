#include <ESP32Servo.h>
#include "DHT.h"
// --- AGREGADO: WiFi y envío a la landing (Vercel) ---
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>   // Librería "ArduinoJson" de Benoit Blanchon (v7)

#define DHTPIN 25
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- AGREGADO: configuración WiFi y servidor ---
const char* WIFI_SSID      = "TU_RED_WIFI";
const char* WIFI_PASSWORD  = "TU_CLAVE_WIFI";
const char* SERVIDOR_URL   = "https://TU-PROYECTO.vercel.app/api/lecturas";
const char* API_TOKEN      = "choco-sena-2026";   // el mismo valor de API_TOKEN en Vercel
const unsigned long INTERVALO_ENVIO = 5000;       // enviar datos cada 5 s
void tareaComunicacion(void* parametro);          // definida al final del archivo

// Variables
Servo miServo;
int sensorAgua = 32;
int nivelAgua = 0;
float humedadAmbiente = 0;
float tempAmbiente = 0;


void setup() {
  Serial.begin(9600);
  miServo.attach(13);
  dht.begin();
  pinMode(sensorAgua, INPUT);

  // --- AGREGADO: la comunicación corre aparte, en el núcleo 0 ---
  xTaskCreatePinnedToCore(tareaComunicacion, "comunicacion", 8192, NULL, 1, NULL, 0);
}

void loop() {

  nivelAgua = analogRead(sensorAgua);
  humedadAmbiente = dht.readHumidity();
  tempAmbiente = dht.readTemperature();

  if(nivelAgua > 500){
    miServo.write(0);
  }
  else{
    miServo.write(160);
  }

  delay(1000);

  Serial.print("Nivel de agua: ");
  Serial.println(nivelAgua);
  Serial.print("Humedad Ambiente: ");
  Serial.println(humedadAmbiente);
  Serial.print("Temperatura Ambiente: ");
  Serial.println(tempAmbiente);
  Serial.println("***********************");


}

// =====================================================================
// --- AGREGADO: tarea que solo envía los datos a Vercel ---
// No toca el servo ni los sensores: lee las variables que actualiza loop().
// Si no hay WiFi, el secadero sigue funcionando normal.
// =====================================================================
void tareaComunicacion(void* parametro) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    JsonDocument doc;
    doc["lluvia"] = nivelAgua;
    if (isnan(humedadAmbiente)) doc["humedad"] = nullptr; else doc["humedad"] = humedadAmbiente;
    if (isnan(tempAmbiente))    doc["temperatura"] = nullptr; else doc["temperatura"] = tempAmbiente;
    String json;
    serializeJson(doc, json);

    WiFiClientSecure cliente;
    cliente.setInsecure();                 // HTTPS sin validar certificado (suficiente para el prototipo)
    HTTPClient http;
    if (http.begin(cliente, SERVIDOR_URL)) {
      http.addHeader("Content-Type", "application/json");
      http.addHeader("x-api-token", API_TOKEN);
      http.setTimeout(8000);
      int codigo = http.POST(json);
      Serial.print("[WiFi] Envio a Vercel: ");
      Serial.println(codigo);              // 201 = recibido
      http.end();
    }

    vTaskDelay(pdMS_TO_TICKS(INTERVALO_ENVIO));
  }
}
