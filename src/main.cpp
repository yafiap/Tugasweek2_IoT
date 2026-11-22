#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ESP32Servo.h>

// ---------- Pins ----------
const int DHT_PIN   = 15;
const int TRIG_PIN  = 2;    // HC-SR04 TRIG  (check in diagram.json)
const int ECHO_PIN  = 16;    // HC-SR04 ECHO  (check in diagram.json)
const int SERVO_PIN = 4;   // Servo signal  (check in diagram.json)

// ---------- Objects ----------
DHTesp dht;
Servo servo;
WiFiClient espClient;
PubSubClient client(espClient);

const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqtt_server = "mqtt-dashboard.com";

unsigned long lastMsg = 0;
bool autoMode = true;         

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

float readDistanceCM()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  if (duration == 0) return -1;                   
  return (duration * 0.0343 / 2.0)-1;                 
}


void callback(char *topic, byte *payload, unsigned int length)
{
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  if (String(topic) == "/tugasweek2_yafi/servo")
  {
    if (msg.equalsIgnoreCase("auto"))
    {
      autoMode = true;
      Serial.println("Servo: AUTO mode");
    }
    else
    {
      int angle = constrain(msg.toInt(), 0, 180);  // send 0..180
      autoMode = false;
      servo.write(angle);
      Serial.print("Servo: MANUAL angle = ");
      Serial.println(angle);
    }
  }
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("\nAttempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str()))
    {
      Serial.println("Connected");
      client.publish("/tugasweek2_yafi/Publish", "Welcome");
      client.subscribe("/tugasweek2_yafi/Subscribe");
      client.subscribe("/tugasweek2_yafi/servo");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(0);

  dht.setup(DHT_PIN, DHTesp::DHT22);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000)
  {
    lastMsg = now;

    TempAndHumidity data = dht.getTempAndHumidity();
    String temp = String(data.temperature, 2);
    String hum  = String(data.humidity, 1);
    client.publish("/tugasweek2_yafi/temp", temp.c_str());
    client.publish("/tugasweek2_yafi/hum", hum.c_str());

    float distance = readDistanceCM();
    String dist = String(distance, 1);
    client.publish("/tugasweek2_yafi/distance", dist.c_str());

    if (autoMode && distance > 0)
    {
      int angle; 
      if(distance < 50.0){
        angle=90;
      }else{
        angle = 0;
      }
      servo.write(angle);
      client.publish("/tugasweek2_yafi/servoState", String(angle).c_str());
    }

    Serial.print("Temperature: "); 
    Serial.println(temp);
    Serial.print("Humidity: ");    
    Serial.println(hum);
    Serial.print("Distance: ");    
    Serial.print(dist); 
    Serial.println(" cm");
  }
}