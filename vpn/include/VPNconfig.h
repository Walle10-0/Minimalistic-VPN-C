#ifndef VPNCONFIG_H
#define VPNCONFIG_H
// shared code between client and server

#include <net/if.h>

// helpful config constants
#define HARDCODED_IP_LENGTH 32
#define DEFAULT_CONFIG_FILE "vpnconfig.cfg"

struct vpn_config {
    // network configuration
    char interfaceName[IFNAMSIZ];
    unsigned short vpnPort; // port for encrypted VPN traffic
    unsigned short tcpPort; // port for TCP control traffic (eg. handshakes, key exchange)

    // public IP addr of server
    char vpnPublicServerIp[HARDCODED_IP_LENGTH]; 

    // dynamic VPN specific configuration
    char vpnClientIp[HARDCODED_IP_LENGTH];
    char vpnPrivateServerIp[HARDCODED_IP_LENGTH]; 
    char vpnNetwork[HARDCODED_IP_LENGTH];
        
    // for asymmetric encryption
    unsigned char * privateKey;
    unsigned char * publicKey;

    // currently only supports hardcoded key
    unsigned char * hardcodedKey; 
};

struct encryptParams {
    unsigned char * key;
    unsigned char * prev_iv;
    size_t iv_len;
    size_t tag_len;
};

struct vpn_context {
    int interfaceFd;
    int vpnSock;
    struct sockaddr_in serverAddr;

    struct encryptParams encryptParams;
};

struct vpn_config readVPNConfig(char * configFilePath);

#endif