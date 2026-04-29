//-----------------------------------------------
//                Tech Random DIY
//               LED_WALL_Receiver
//                 Chris Parker
//-----------------------------------------------
// WebSocket Globals
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>

// CHANGE THIS PER BOARD: 1 for the left panel, 2 for the right panel
#define DEVICE_NUM 2

#define _STR_HELPER(x) #x
#define _STR(x) _STR_HELPER(x)
#define DEVICE_MSG "Device " _STR(DEVICE_NUM)

WebSocketsClient webSocket;
#define PAYLOAD_MAX 0x1F
#define PAYLOAD_MAX_G 0x3F
//-----------------------------------------------
// FastLED Gloabals
#include <FastLED.h>
#define WIDTH 16
#define HEIGHT 32
#define LED_PIN 2
#define MAX_BRIGHTNESS 200

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
 *    On connection the server will ask "Who?" and expect a 
 *    response with the device number.
 *    Receives data from the server as a payload,
 *    then store that data into the LED array buffer before
 *    pushing the pixel data to the display.
 *****************************************************************/
void webSocketEvent(WStype_t type, uint8_t * payload, size_t welength) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected from server");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected as " DEVICE_MSG "\n");
      break;
    case WStype_TEXT:
      if (strcmp((char *)payload, "Who?") == 0){
        webSocket.sendTXT(DEVICE_MSG);
        return;
      }
      for (int i = 0; i < NUM_LEDS; i++){
        int red = *(payload + (2*i)) >> 3;
        int green = ((*(payload + (2*i)) & 0x07) << 3) | (*(payload + (2*i) + 1) >> 5);
        int blue = *(payload + (2*i) + 1) & 0x1F;
        leds[i].r = map(red, 0, PAYLOAD_MAX, 0, MAX_BRIGHTNESS);
        leds[i].g = map(green, 0, PAYLOAD_MAX_G, 0, MAX_BRIGHTNESS);
        leds[i].b = map(blue, 0, PAYLOAD_MAX, 0, MAX_BRIGHTNESS);
      }
      FastLED.show();
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
	// server address, port and URL — LMCSHD PC, configured via secrets.h
	webSocket.begin(PC_IP, PC_PORT, "/");
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
