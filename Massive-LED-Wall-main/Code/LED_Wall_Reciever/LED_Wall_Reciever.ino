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
// Reply format on connect: "Device <num> <W>x<H>" — LMCSHD parses both pieces
// to know which slot the connection claims AND the panel's pixel dimensions
// (Feature 6.9). WIDTH and HEIGHT come from the FastLED globals defined
// below; the macro indirection forces them to expand before stringification.
#define DEVICE_MSG "Device " _STR(DEVICE_NUM) " " _STR(WIDTH) "x" _STR(HEIGHT)

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

/* Feature 6.8 (abandoned 2026-05-01) — app-level watchdog tracking the time
 * since the last received WS message. Paired with an LMCSHD-side keepalive
 * timer pushing every 1s. Worked for LMCSHD-crash detection but didn't fire
 * on Windows hibernate (suspect: OS keeps the NIC awake enough that LMCSHD's
 * pre-hibernate frame queue still gets transmitted, then no further detection
 * triggers). Abandoned per user request; hibernate now leaves the wall in
 * its last state until something else trips a disconnect. Left commented as
 * a starting point if revisited.
 *
 * unsigned long lastDataTime = 0;
 * const unsigned long DATA_TIMEOUT_MS = 3000;
 */
//-----------------------------------------------
#include "secrets.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
//-----------------------------------------------

/*****************************************************************
 *    decodeBPP16AndShow()
 *
 *    Unpack 5-6-5 RGB pixels from `payload` into the FastLED buffer
 *    and push to the display. Called from WStype_BIN (LMCSHD sends
 *    pixel data as proper binary frames). Sends a single 0x06 ack
 *    byte back to the server after FastLED.show() so the PC can
 *    throttle frame production to the slowest receiver and keep
 *    panels in lockstep.
 *****************************************************************/
void decodeBPP16AndShow(uint8_t *payload) {
  for (int i = 0; i < NUM_LEDS; i++){
    int red = *(payload + (2*i)) >> 3;
    int green = ((*(payload + (2*i)) & 0x07) << 3) | (*(payload + (2*i) + 1) >> 5);
    int blue = *(payload + (2*i) + 1) & 0x1F;
    leds[i].r = map(red, 0, PAYLOAD_MAX, 0, MAX_BRIGHTNESS);
    leds[i].g = map(green, 0, PAYLOAD_MAX_G, 0, MAX_BRIGHTNESS);
    leds[i].b = map(blue, 0, PAYLOAD_MAX, 0, MAX_BRIGHTNESS);
  }
  FastLED.show();
  uint8_t ack = 0x06;
  webSocket.sendBIN(&ack, 1);
}

/*****************************************************************
 *    webSocketEvent()
 *    Parameters: WStype_t type, uint8_t * payload, size_t length
 *    Returns: void
 *
 *    On connection the server asks "Who?" and expects a "Device N"
 *    text reply. Pixel frames arrive as binary (WStype_BIN); text
 *    frames are reserved for the handshake.
 *****************************************************************/
void webSocketEvent(WStype_t type, uint8_t * payload, size_t welength) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected from server");
      // Belt-and-suspenders blank: covers cases where LMCSHD didn't get a
      // chance to push a black frame before the connection died (hard PC
      // shutdown, sleep, crash, network glitch). LMCSHD blanks proactively
      // on lock/sleep/disconnect — this just guarantees the wall goes dark
      // even when the PC-side handler couldn't run.
      FastLED.clear();
      FastLED.show();
      // lastDataTime = 0;  // Feature 6.8 (abandoned)
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected as " DEVICE_MSG "\n");
      // lastDataTime = millis();  // Feature 6.8 (abandoned)
      break;
    case WStype_TEXT:
      // lastDataTime = millis();  // Feature 6.8 (abandoned)
      if (strcmp((char *)payload, "Who?") == 0)
        webSocket.sendTXT(DEVICE_MSG);
      break;
    case WStype_BIN:
      // lastDataTime = millis();  // Feature 6.8 (abandoned)
      decodeBPP16AndShow(payload);
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

  /* Feature 6.8 (abandoned 2026-05-01) — WS-protocol heartbeat. Was meant
   * to detect silent failures by pinging the server periodically and
   * disconnecting when pongs stopped arriving. In practice it didn't fire
   * on Windows hibernate (suspect: OS keeps the WiFi NIC alive enough to
   * make the WS pings appear to succeed even though no app is running on
   * the PC to actually pong them). Combined with the v2 app-level watchdog,
   * still didn't blank the wall on hibernate. Abandoned per user request.
   *
   * webSocket.enableHeartbeat(5000, 2000, 2);
   */
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

	/* Feature 6.8 (abandoned 2026-05-01) — app-level liveness check. See
	 * the globals comment near the top of the file for rationale.
	 *
	 * if (lastDataTime != 0 && (millis() - lastDataTime) > DATA_TIMEOUT_MS) {
	 *   Serial.println("[WS] Data watchdog timeout — disconnecting and blanking");
	 *   FastLED.clear();
	 *   FastLED.show();
	 *   webSocket.disconnect();
	 *   lastDataTime = 0;
	 * }
	 */
}
