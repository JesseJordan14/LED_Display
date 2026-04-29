//-----------------------------------------------
//                Tech Random DIY
//               LED_WALL_Receiver
//                 Chris Parker
//-----------------------------------------------
// WebSocket Globals
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>

WebSocketsClient webSocket;
//-----------------------------------------------
// FastLED Gloabals
#include <FastLED.h>
#define WIDTH 16
#define HEIGHT 32
#define LED_PIN 2

const int NUM_LEDS = WIDTH * HEIGHT;
CRGB leds[NUM_LEDS];
//-----------------------------------------------
#include "secrets.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
//-----------------------------------------------

/*****************************************************************    
 *    webSocketEvent()
 *    Parameters: WStype_t type, uint8_t * payload, size_t length
 *    Returns: void
 * 
 *    Receive data from the server as a payload,
 *    then store that data into the LED array buffer before
 *    pushing the pixel data to the display.
 *****************************************************************/
void webSocketEvent(WStype_t type, uint8_t * payload, size_t welength) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected from server");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to server: %s\n", payload);
      break;
    case WStype_TEXT:
      for (int i = 0; i < NUM_LEDS; i++){
        leds[i].r = *(payload + (3*i));
        leds[i].g = *(payload + (3*i) + 1);
        leds[i].b = *(payload + (3*i) + 2);
      }
      FastLED.show();
      break;
    case WStype_ERROR:
      Serial.println("[WSc] Error");
      break;
    default:
      break;
  }
}
/*****************************************************************    
 *    setup()
 * 
 *    Start serial, connect to WiFi, then start the websocket.
 *****************************************************************/
void setup() {
	Serial.begin(115200);
  // Initialize LED Array
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
//-----------------------------------------------
  // Connect to WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
	WiFi.begin(ssid, password);

	while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
		delay(500);
	}
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
//-----------------------------------------------
	// server address, port and URL
	webSocket.begin("10.0.0.121", 81, "/");
	// event handler
	webSocket.onEvent(webSocketEvent);
  // try again if connection has failed
  webSocket.setReconnectInterval(5000);
//-----------------------------------------------
  leds[0].r = 100;
  FastLED.show();
}
/*****************************************************************    
 *    loop()
 * 
 *    Handles web socket.
 *****************************************************************/
void loop() {
	webSocket.loop();
}
