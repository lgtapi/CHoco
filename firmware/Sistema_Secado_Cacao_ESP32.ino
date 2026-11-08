#include <ESP32Servo.h>
#include "DHT.h"

#define DHTPIN 25
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

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
