#include <Wire.h>
#include <Adafruit_GFX.h>
#include <WiFiNINA.h>   
#include <PubSubClient.h>
#include <utility/wifi_drv.h>   // library to drive the RGB LED on the MKR1010
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050_tockn.h>

#include "arduino_secrets.h" 
#include <Adafruit_NeoPixel.h>

#define PIN 6
#define NUMPIXELS 8
#define DELAYVAL 500

Adafruit_NeoPixel pixels(NUMPIXELS, PIN);

/*
**** Please enter your sensitive data in the Secret tab/arduino_secrets.h
**** using the format below

#define SECRET_SSID "ssid name"
#define SECRET_PASS "ssid password"
#define SECRET_MQTTUSER "user name - eg student"
#define SECRET_MQTTPASS "password";
 */

const char* ssid          = SECRET_SSID;
const char* password      = SECRET_PASS;
const char* mqtt_username = SECRET_MQTTUSER;
const char* mqtt_password = SECRET_MQTTPASS;
const char* mqtt_server = "mqtt.cetools.org";
const int mqtt_port = 1884;
int status = WL_IDLE_STATUS;     // the Wifi radio's status
int lightsatus = 0;

// Define button pins
const int button1Pin = 2; // Button 1 connected to D2
const int button2Pin = 3; // Button 2 connected to D3
const int button3Pin = 4; // Button 3 connected to D4
const int button4Pin = 5; // Button 4 connected to D5

// Button count variables
int button1Count = 0;
int button2Count = 0;
int button3Count = 0;
int button4Count = 0;

// Last button state
bool lastButton1State = HIGH;
bool lastButton2State = HIGH;
bool lastButton3State = HIGH;
bool lastButton4State = HIGH;

MPU6050 mpu6050(Wire);

struct gyro{
  float posX;
  float posY;
  float posZ;
};

gyro LatestPos;

struct RGBValue{
  int Red;
  int Blue;
  int Green;
  int clrstatus;
};

RGBValue Colors;
RGBValue Rainbow[12] = {
  {255, 0, 0, 5},       // Red
  {255, 127, 0, 5},     // Orange
  {255, 255, 0, 5},     // Yellow
  {127, 255, 0, 5},     // Yellow-green
  {0, 255, 0, 5},       // Green
  {0, 255, 127, 5},     // Cyan-green
  {0, 255, 255, 5},     // Cyan
  {0, 127, 255, 5},     // Blue
  {0, 0, 255, 5},       // Indigo
  {127, 0, 255, 5},     // Violet
  {255, 0, 255, 5},     // Magenta
  {255, 0, 127, 5}      // Red-violet
};

WiFiServer server(80);
WiFiClient wificlient;

WiFiClient mkrClient;
PubSubClient client(mkrClient);

// Edit this for the light you are connecting to
char mqtt_topic_demo[] = "student/CASA0014/light/2/pixel/";

void setup() {
  // Start the serial monitor to show output
  Serial.begin(115200);
  delay(1000);

  // Set button pins to input mode and enable internal pull-up resistors
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(button3Pin, INPUT_PULLUP);
  pinMode(button4Pin, INPUT_PULLUP);

  WiFi.setHostname("Lumina ucjtdjw");
  startWifi();
  client.setServer(mqtt_server, mqtt_port);
  pixels.begin();
  pixels.setBrightness(150); // Set brightness, range from 0 (darkest) to 255 (brightest)

  while (!Serial);

  // Initialize I2C communication
  Wire.begin();
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);

  Serial.println("setup complete");
}

void loop() {

  // We need to make sure the Arduino is still connected to the MQTT broker
  // Otherwise, we will not receive any messages
  if (!client.connected()) {
    reconnectMQTT();
  }

  // We also need to make sure we are connected to the Wi-Fi
  // Otherwise, it will be impossible to connect to MQTT!
  if (WiFi.status() != WL_CONNECTED){
    startWifi();
  }

  // Check for messages from the broker and ensure that any outgoing messages are sent.
  client.loop();

  mpu6050.update();
  colorUpdate();

  buttonpress();
  //sendsinglepxmqtt();
  //sendgroupmqtt();
  Serial.println("sent a message");
  //delay(100);
  //delay(100);
  
}

void resetLight(){
  char mqtt_message[100];
  char mqtt_topic_gothrough[] = "student/CASA0014/light/1/pixel/";
  for(int j = 1; j < 53; j++){
      sprintf(mqtt_topic_gothrough, "student/CASA0014/light/%d/all/",j);
      for(int i = 0; i < 12; i++){
        sprintf(mqtt_message, "{\"method\": \"clear\"}");
        //sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": 255, \"G\": 0, \"B\": 0, \"W\": 0}", i);

        Serial.println(mqtt_topic_gothrough);
        Serial.println(mqtt_message);

        if (client.publish(mqtt_topic_gothrough, mqtt_message)) {
          Serial.println("Message published");
        } else {
          Serial.println("Failed to publish message");
        }
      }
  }
}

void buttonpress() {
  // Read button states
  bool button1State = digitalRead(button1Pin) == LOW;
  bool button2State = digitalRead(button2Pin) == LOW;
  bool button3State = digitalRead(button3Pin) == LOW;
  bool button4State = digitalRead(button4Pin) == LOW;

  // Detect button 1's state change (from not pressed to pressed)
  if (button1State && !lastButton1State) {
    if (button1Count == 0){
      button1Count++;
    } else if (button1Count == 1){
      button1Count = 0;
    }
  }

  // Detect button 2's state change
  if (button2State && !lastButton2State) {
    button2Count++;
    resetLight();
  }

  // Detect button 3's state change
  if (button3State && !lastButton3State) {
    button3Count++;
  }

  // Detect button 4's state change
  if (button4State && !lastButton4State) {
    button4Count++;
  }

  // Update the last button state
  lastButton1State = button1State;
  lastButton2State = button2State;
  lastButton3State = button3State;
  lastButton4State = button4State;

  if (button1Count == 0){
    sendsinglepxmqtt();
  } else if (button1Count == 1){
    sendgroupmqtt();
  }

  // Print button state and count to the serial monitor
  Serial.print("Button 1: ");
  Serial.print(button1State ? "Pressed" : "Released");
  Serial.print(" (Count: ");
  Serial.print(button1Count);
  Serial.print(") | Button 2: ");
  Serial.print(button2State ? "Pressed" : "Released");
  Serial.print(" (Count: ");
  Serial.print(button2Count);
  Serial.print(") | Button 3: ");
  Serial.print(button3State ? "Pressed" : "Released");
  Serial.print(" (Count: ");
  Serial.print(button3Count);
  Serial.print(") | Button 4: ");
  Serial.print(button4State ? "Pressed" : "Released");
  Serial.print(" (Count: ");
  Serial.print(button4Count);
  Serial.println(")");
}


void sendgroupmqtt(){
  // send a message to update the light
  char mqtt_message[100];
  char mqtt_topic_gothrough[] = "student/CASA0014/light/1/pixel/";
  
  if (lightsatus != Colors.clrstatus)
  {
    for(int j = 1; j < 53; j++){
      sprintf(mqtt_topic_gothrough, "student/CASA0014/light/%d/pixel/",j);
      for(int i = 0; i < 12; i++){
        sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": %d, \"G\": %d, \"B\": %d, \"W\": 0}", i, Colors.Red, Colors.Green, Colors.Blue);
        //sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": 255, \"G\": 0, \"B\": 0, \"W\": 0}", i);

        Serial.println(mqtt_topic_gothrough);
        Serial.println(mqtt_message);
        

        if (client.publish(mqtt_topic_gothrough, mqtt_message)) {
          Serial.println("Message published");
        } else {
          Serial.println("Failed to publish message");
        }
      }
      pixels.clear();
      for(int i=0; i<NUMPIXELS; i++) {
        pixels.setPixelColor(i, Colors.Red, Colors.Green, Colors.Blue);
        pixels.show();
      }
      lightsatus = Colors.clrstatus;
    }
  }
}

void sendsinglepxmqtt(){

  // send a message to update the light
  char mqtt_message[100];
  
  if (lightsatus != Colors.clrstatus)
  {
    for(int i = 0; i < 12; i++){
      sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": %d, \"G\": %d, \"B\": %d, \"W\": 0}", i, Colors.Red, Colors.Green, Colors.Blue);
      //sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": 255, \"G\": 0, \"B\": 0, \"W\": 0}", i);

      Serial.println(mqtt_topic_demo);
      Serial.println(mqtt_message);
      

      if (client.publish(mqtt_topic_demo, mqtt_message)) {
        Serial.println("Message published");
      } else {
        Serial.println("Failed to publish message");
      }
    }
    pixels.clear();
    for(int i=0; i<NUMPIXELS; i++) {
      pixels.setPixelColor(i, Colors.Red, Colors.Green, Colors.Blue);
      pixels.show();
    }
    lightsatus = Colors.clrstatus;
  }
}

void startWifi(){
    
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // Function for connecting to a WiFi network
  // is looking for UCL_IoT and a back up network (usually a home one!)
  int n = WiFi.scanNetworks();
  Serial.println("Scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    // loop through all the networks and if you find UCL_IoT or the backup - ssid1
    // then connect to wifi
    Serial.print("Trying to connect to: ");
    Serial.println(ssid);
    for (int i = 0; i < n; ++i){
      String availablessid = WiFi.SSID(i);
      // Primary network
      if (availablessid.equals(ssid)) {
        Serial.print("Connecting to ");
        Serial.println(ssid);
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
          delay(600);
          Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Connected to " + String(ssid));
          break; // Exit the loop if connected
        } else {
          Serial.println("Failed to connect to " + String(ssid));
        }
      } else {
        Serial.print(availablessid);
        Serial.println(" - this network is not in my list");
      }

    }
  }


  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED){
    startWifi();
  } else {
    //Serial.println(WiFi.localIP());
  }

  // Loop until we're reconnected
  while (!client.connected()) {    // while not (!) connected....
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "LuminaSelector";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // ... and subscribe to messages on broker
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, int length) {
  // Handle incoming messages
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

void colorUpdate(){
  mpu6050.update();

  LatestPos.posX = mpu6050.getAngleX();
  LatestPos.posY = mpu6050.getAngleY();
  LatestPos.posZ = mpu6050.getAngleZ();

  if (LatestPos.posX >= -45 & LatestPos.posX < 45)
  {
    if(LatestPos.posY >= 45 & LatestPos.posY <= 90)
    {
      Colors.Red = 255;
      Colors.Green = 255;
      Colors.Blue = 0;
      Colors.clrstatus = 4;
    }else if (LatestPos.posY >= -45 & LatestPos.posY <= 45){
      if (lightsatus != 5){
        char mqtt_message[100];
        char mqtt_topic_gothrough[] = "student/CASA0014/light/1/pixel/";

        if (button1Count == 0){
          for(int i = 0; i < 12; i++){
            sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": %d, \"G\": %d, \"B\": %d, \"W\": 0}", i, Rainbow[i].Red, Rainbow[i].Green, Rainbow[i].Blue);
            //sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": 255, \"G\": 0, \"B\": 0, \"W\": 0}", i);

            Serial.println(mqtt_topic_demo);
            Serial.println(mqtt_message);
            

            if (client.publish(mqtt_topic_demo, mqtt_message)) {
              Serial.println("Message published");
            } else {
              Serial.println("Failed to publish message");
            }
          }
          pixels.clear();
          for(int i=0; i<NUMPIXELS; i++) {
            pixels.setPixelColor(i, Rainbow[i].Red, Rainbow[i].Green, Rainbow[i].Blue);
            pixels.show();
          }
          lightsatus = 5;
          Colors.clrstatus = 5;
        }else if (button1Count == 1){
          for(int j = 1; j < 53; j++){
            sprintf(mqtt_topic_gothrough, "student/CASA0014/light/%d/pixel/",j);
            for(int i = 0; i < 12; i++){
              sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": %d, \"G\": %d, \"B\": %d, \"W\": 0}", i, Rainbow[i].Red, Rainbow[i].Green, Rainbow[i].Blue);
              //sprintf(mqtt_message, "{\"pixelid\": %d, \"R\": 255, \"G\": 0, \"B\": 0, \"W\": 0}", i);

              Serial.println(mqtt_topic_gothrough);
              Serial.println(mqtt_message);
              

              if (client.publish(mqtt_topic_gothrough, mqtt_message)) {
                Serial.println("Message published");
              } else {
                Serial.println("Failed to publish message");
              }
            }
            pixels.clear();
            for(int i=0; i<NUMPIXELS; i++) {
              pixels.setPixelColor(i, Rainbow[i].Red, Rainbow[i].Green, Rainbow[i].Blue);
              pixels.show();
            }
            lightsatus = 5;
            Colors.clrstatus = 5;
          }
        }
      }
    }else if(LatestPos.posY >= -135 & LatestPos.posY < -45){
      Colors.Red = 255;
      Colors.Green = 0;
      Colors.Blue = 0;
      Colors.clrstatus = 1;
    }

  }else if(LatestPos.posX >= 45 & LatestPos.posX < 135){
    Colors.Red = 0;
    Colors.Green = 255;
    Colors.Blue = 0;
    Colors.clrstatus = 2;
  }else if (LatestPos.posX >= -135 & LatestPos.posX < -45){
    Colors.Red = 0;
    Colors.Green = 0;
    Colors.Blue = 255;
    Colors.clrstatus = 3;
  }else{
    Colors.Red = 255;
    Colors.Green = 255;
    Colors.Blue = 0;
    Colors.clrstatus = 4;
  }
  Serial.print("angleX : ");
  Serial.print(mpu6050.getAngleX());
  Serial.print("\tangleY : ");
  Serial.print(mpu6050.getAngleY());
  Serial.print("\tangleZ : ");
  Serial.println(mpu6050.getAngleZ());
  Serial.print("R: ");
  Serial.print(Colors.Red);
  Serial.print("\tG: ");
  Serial.print(Colors.Green);
  Serial.print("\tB : ");
  Serial.println(Colors.Blue);
  //delay(1500);
}
