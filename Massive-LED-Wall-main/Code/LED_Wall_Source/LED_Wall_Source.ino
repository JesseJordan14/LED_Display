//-----------------------------------------------
//                Tech Random DIY
//               LED_WALL_Source
//                 Chris Parker
//-----------------------------------------------
// WebSocket Globals
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

#include "html.h"   //Seperate file with webpage
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
//-----------------------------------------------
// LED Gloabals
#define WIDTH 32
#define HEIGHT 32
#define NUM_PANELS 2
#define PANEL_WIDTH (WIDTH / NUM_PANELS)
const int NUM_LEDS = WIDTH * HEIGHT;
const int PANEL_BYTES = (NUM_LEDS / NUM_PANELS) * 2;
const int FRAME_BYTES = NUM_LEDS * 2;
char fullFrame[FRAME_BYTES];
char panel1[PANEL_BYTES];
char panel2[PANEL_BYTES];
//-----------------------------------------------
// Client Globals (One for each screen)
uint8_t screen1 = 99;
uint8_t screen2 = 99;
//-----------------------------------------------
#include "secrets.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
//-----------------------------------------------
// Set a Static IP address
IPAddress local_IP(10, 0, 0, 121);
// Set a Gateway IP address
IPAddress gateway(10, 0, 0, 1);
IPAddress subnet(255, 255, 255, 0);
//-----------------------------------------------
/*****************************************************************    
 *    webSocketEvent()
 * 
 *    For now the server only sends data so this is empty.
 *****************************************************************/
void webSocketEvent(uint8_t clientNum, WStype_t type, uint8_t *payload, size_t welength) {
  switch(type) {
    case WStype_DISCONNECTED:
      disconnectClient(clientNum);
      break;
    case WStype_CONNECTED:
      webSocket.sendTXT(clientNum, "Who?");
      break;
    case WStype_TEXT:
      // the payload is a string containing the message from the client
      String message = String((char*)payload);
      // check if the message starts with "Device "
      if (message.startsWith("Device ")) {
        // extract the device number from the message
        saveClientNum(clientNum, message.substring(7).toInt());
      }
      break;
  }
}
/*****************************************************************    
 *    webpage()
 * 
 *    This function sends the webpage to clients
 *****************************************************************/
void webpage()
{
  server.send(200,"text/html", htmlCode);
}
/*****************************************************************    
 *    setDeviceNum()
 * 
 *    This function saves each screens clients number
 *****************************************************************/
void saveClientNum(int clientNum, int screenNum){
  switch (screenNum){
    case 1:
      screen1 = clientNum;
      webSocket.broadcastTXT("C1");
      break;
    case 2:
      screen2 = clientNum;
      webSocket.broadcastTXT("C2");
      break;
  }
  return;
}
/*****************************************************************    
 *    disconnectClient()
 * 
 *    Broadcast the client disconnect message
 *****************************************************************/
void disconnectClient(int clientNum){
  webSocket.broadcastTXT("Disconnected");
  if (clientNum == screen1)
      webSocket.broadcastTXT("D1");
  if (clientNum == screen2)
      webSocket.broadcastTXT("D2");
}
/*****************************************************************    
 *    setup()
 * 
 *    Connect to WiFi, start the server, start the websocket 
 *    connection, then start serial. Serial connection must be as 
 *    fast as possible for the best frame rate.
 *****************************************************************/
void setup() {
  //Configure static IP address
  WiFi.config(local_IP, gateway, subnet);
  
  // Connect to Wi-Fi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  server.on("/", webpage);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.begin(921600);
}
/*****************************************************************    
 *    loop()
 * 
 *    Handles server and web socket. Reads data over serial then
 *    broadcasts it to all clients.
 *****************************************************************/
void loop() {
  server.handleClient();
  webSocket.loop();
  // Read header from LMCSDH
  switch(Serial.read()){ 
    case 0x05: // Request for matrix definition
      Serial.println(WIDTH);
      Serial.println(HEIGHT);
      break;
    case 0x42: // Read frame data
      // Read the entire 32x32 frame from LMCSHD as one block.
      // LMCSHD orientation: horizontal, origin bottom-right, snake.
      Serial.readBytes(fullFrame, FRAME_BYTES);

      // Demux into per-panel buffers. Each receiver expects its 16x32 region
      // in the same horizontal/bottom-right/snake order that one panel uses.
      // Local index kL -> row r (from bottom), col c (within row).
      // For the LEFT panel (global x in [0,15]):
      //   even row (right-to-left): global col = (PANEL_WIDTH + c)
      //   odd  row (left-to-right): global col = c
      // For the RIGHT panel (global x in [16,31]) the cases swap.
      for (int kL = 0; kL < NUM_LEDS / NUM_PANELS; kL++) {
        int r = kL / PANEL_WIDTH;
        int c = kL % PANEL_WIDTH;
        bool evenRow = ((r & 1) == 0);
        int gColLeft  = evenRow ? (PANEL_WIDTH + c) : c;
        int gColRight = evenRow ? c : (PANEL_WIDTH + c);
        int kGleft  = r * WIDTH + gColLeft;
        int kGright = r * WIDTH + gColRight;
        panel1[2*kL]     = fullFrame[2*kGleft];
        panel1[2*kL + 1] = fullFrame[2*kGleft + 1];
        panel2[2*kL]     = fullFrame[2*kGright];
        panel2[2*kL + 1] = fullFrame[2*kGright + 1];
      }

      webSocket.sendTXT(screen1, (char *)panel1, PANEL_BYTES);
      webSocket.sendTXT(screen2, (char *)panel2, PANEL_BYTES);

      Serial.write(0x06); //acknowledge
      break;
  }
}
