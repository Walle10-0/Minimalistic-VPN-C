#ifndef VPNTOOLS_H
#define VPNTOOLS_H

#include <netinet/in.h>

#define TUNTAP_NAME "vpnclient"
#define VPN_CLIENT_IP "10.8.0.2/24"
#define VPN_PRIVATE_SERVER_IP "10.8.0.1/24"
#define VPN_PUBLIC_SERVER_IP "192.168.122.85"
#define VPN_PORT 55555

#define MAX_BUF_SIZE 2000
#define MAXPENDING 5

struct vpn_context {
    int interfaceFd;
    int vpnSock;
    struct sockaddr_in serverAddr;
    // encryption keys, etc.
};

// I should use this more often ... or not
void DieWithError(char *errorMessage);

#endif