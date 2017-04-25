#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

#define WLAN_SSID       "Florin"//"Dekart-base"
#define WLAN_PASS       "25834florin"//"gubi08tude"
#define AIO_SERVER      "192.168.1.5" //"broker.mqttdashboard.com"
#define AIO_SERVERPORT  1883 // use 8883 for SSL
#define AIO_USERNAME    ""
#define AIO_KEY         ""

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Setup a feed called 'publisher' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish publisher = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "jvompiha");

// Setup a feed called 'subscriber' for subscribing to changes.
Adafruit_MQTT_Subscribe subscriber = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "testtopic/2");

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

SoftwareSerial mySerial(12, 13); // RX, TX
char* CON_COMMAND = "AT+CON";
char* RESET_COMMAND = "AT+RESET";
char* BATTERY_LEVEL_COMMAND = "AT+BATT?";
char* device_address = "544A1625B97C";
char full_conn_command[20];

String get_response() {
  String buffer = "";
  while(mySerial.available()){
    buffer += (char)mySerial.read();
  }
  return buffer;
}

bool device_connect(){
  Serial.printf("\nConnecting %s...", device_address);
  strcpy(full_conn_command, CON_COMMAND);
  strcat(full_conn_command, device_address);
  mySerial.write(full_conn_command);
  delay(100);
  int buffer_size = mySerial.available();
  String buffer = get_response();
  Serial.print("buffer: "); 
  Serial.println(buffer);
  delay(1000);
  if(mySerial.available() == 7){
    Serial.println(get_response());
    return true;
  }
  mySerial.write(RESET_COMMAND);
  delay(300);
  while(mySerial.available()){
    Serial.write(mySerial.read());
  }
  return false;
}

void get_battery_level(char* battery_level){
  mySerial.write(BATTERY_LEVEL_COMMAND);
  delay(500);
  String buffer = get_response();
  battery_level[3] = '\0';
  int index = 0;
  for(int i=7; i<10; i++){
    battery_level[index] = buffer[i];
    index++;
  }
}

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  delay(10);
  Serial.println("Starting!");
  
  // Connect to WiFi access point.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  // Setup MQTT subscription for subscriber feed.
  mqtt.subscribe(&subscriber);

  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);

}

void loop() { // run over and over
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();
  if(device_connect()){
    // Memory pool for JSON object tree.
    //
    // Inside the brackets, 200 is the size of the pool in bytes.
    // If the JSON object is more complex, you need to increase that value.
    StaticJsonBuffer<200> jsonBuffer;
    // Create the root of the object tree.
    //
    // It's a reference to the JsonObject, the actual bytes are inside the
    // JsonBuffer with all the other nodes of the object tree.
    // Memory is freed when jsonBuffer goes out of scope.
    JsonObject& json = jsonBuffer.createObject();

    Serial.println("\nasking battery...");
    char battery_level[4] = {0,0,0,0};
    get_battery_level(battery_level);
    Serial.printf("\nbatt: %s", battery_level);
    json["battery"] = String(battery_level);
    char temp_json[200];
    json.printTo(temp_json);

    // Now we can publish stuff!
    Serial.print(F("\nSending data "));
    Serial.print(battery_level);
    Serial.print("...");
    if (!publisher.publish(temp_json)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));
    }

    Serial.println("\nexiting...");
    mySerial.write("AT");
    delay(500);
    Serial.println(get_response());  
  }

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

//  Adafruit_MQTT_Subscribe *subscription;
//  while ((subscription = mqtt.readSubscription(5000))) {
//    if (subscription == &subscriber) {
//      Serial.print(F("Got: "));
//      Serial.println((char *)subscriber.lastread);
//    }
//  }

  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  /*
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  */

}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}



//#include <SoftwareSerial.h>
//
//SoftwareSerial swSer(12, 13);
//
//void setup() {
//  Serial.begin(9600);
//  swSer.begin(9600);
//
//  Serial.println("\nSoftware serial test started");
//
//  for (char ch = ' '; ch <= 'z'; ch++) {
//    swSer.write(ch);
//  }
//  swSer.println("");
//
//}
//
//void loop() {
//  while (swSer.available() > 0) {
//    Serial.write(swSer.read());
//  }
//  while (Serial.available() > 0) {
//    swSer.write(Serial.read());
//  }
//
//}
