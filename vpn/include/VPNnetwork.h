#ifndef VPNNETWORK_H
#define VPNNETWORK_H
// shared code between client and server

// libraries
#include <netinet/in.h>
#include <netlink/netlink.h>

#include "VPNconfig.h"

// buffer constants
#define MAX_VPN_CLIENTS 20
#define MAX_BUF_SIZE 2000
#define MAXPENDING 5

// function prototypes

int setupUDPSocket(unsigned short fileServPort);
int setupTCPSocket(unsigned short fileServPort);

int setServerAddress(struct vpn_context * context, char * serverIP, unsigned short serverPort);

int createInterface(char *interfaceName, char * ipAddr, struct vpn_config * config, int (*specialConfiguration)(struct nl_sock *, char *, struct vpn_config *));

void setupVPNContext(struct vpn_context * context, char * ipAddr, struct vpn_config * config, int (*specialConfiguration)(struct nl_sock *, char *, struct vpn_config *));

#endif