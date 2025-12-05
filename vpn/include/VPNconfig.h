#ifndef VPNCONFIG_H
#define VPNCONFIG_H
// shared code between client and server

#include <net/if.h>

// helpful config constants
#define HARDCODED_IP_LENGTH 32
#define DEFAULT_CONFIG_FILE "vpnconfig.cfg"

struct vpn_config {
    char interfaceName[IFNAMSIZ];
    char vpnClientIp[HARDCODED_IP_LENGTH];
    char vpnPublicServerIp[HARDCODED_IP_LENGTH]; 
    char vpnPrivateServerIp[HARDCODED_IP_LENGTH]; 
    char vpnNetwork[HARDCODED_IP_LENGTH];
    unsigned short vpnPort;
    // unsigned int maxVpnClients; // currently unused
    unsigned char * hardcodedKey; // currently only supports hardcoded key
};

struct vpn_config readVPNConfig(char * configFilePath);

#endif