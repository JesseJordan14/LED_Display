#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "your_network_name"
#define WIFI_PASSWORD "your_network_password"

// LMCSHD PC running the WebSocket server.
// Reserve a static DHCP lease for the PC so this stays correct across reboots.
#define PC_IP "10.0.0.???"
#define PC_PORT 81

#endif
