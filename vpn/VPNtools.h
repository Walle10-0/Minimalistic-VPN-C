#ifndef VPNTOOLS_H
#define VPNTOOLS_H
// shared code between client and server

// libraries
#include <netinet/in.h>
#include <netlink/netlink.h>

// VPN config constants
#define TUNTAP_NAME "vpnlink0"
#define VPN_CLIENT_IP "10.8.0.2/24"
#define VPN_PRIVATE_SERVER_IP "10.8.0.1/24"
#define VPN_NETWORK "10.8.0.0/24"
#define VPN_PUBLIC_SERVER_IP "192.168.122.85"
#define VPN_PORT 55555
#define MAX_VPN_CLIENTS 20
#define HARDCODED_KEY "1e27bbe48a548a9cd4ab0b86b04bb8c4220d3e179d45e07a90f3467e63d15b8c"

// buffer constants
#define MAX_BUF_SIZE 2000
#define MAXPENDING 5

// structs
struct vpn_context {
    int interfaceFd;
    int vpnSock;
    struct sockaddr_in serverAddr;
    // encryption keys, etc.
};

// function prototypes

// I should use this more often ... or not
void DieWithError(char *errorMessage);

int setupUDPSocket(unsigned short fileServPort);

int setServerAddress(struct vpn_context * context, char * serverIP, unsigned short serverPort);

int createInterface(char *interfaceName, char * ipAddr, int (*specialConfiguration)(struct nl_sock *, char *));

void setupVPNContext(struct vpn_context * context, char * ipAddr, int (*specialConfiguration)(struct nl_sock *, char *));

// hook for later encryption and decryption
int encryptData(unsigned char * inputBuf, ssize_t inputLen, unsigned char * outputBuf, ssize_t * outputLen, const unsigned char * key, const unsigned char * iv);

int decryptData(unsigned char * inputBuf, ssize_t inputLen, unsigned char * outputBuf, ssize_t * outputLen, const unsigned char * key, const unsigned char * iv);

#endif