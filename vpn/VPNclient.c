#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

// Linux TUN/TAP headers
#include <net/if.h>
#include <linux/if_tun.h>

// netlink libraries
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <netlink/route/route.h>
#include <netlink/route/addr.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// system headers
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>  // for multi-threading

// my libraries/code
#include "VPNtools.h"
#include "VPNcrypt.h"
#include "VPNnetwork.h"
#include "VPNconfig.h"

unsigned char * key = NULL;

int addClientRoutingRules(struct nl_sock *sock, char *vpnIfName, struct vpn_config * config)
{
    int err = 0;
    // re-route all traffic through the VPN
    struct rtnl_route *route = rtnl_route_alloc();
    struct nl_addr *dst, *gateway;

    // default route
    nl_addr_parse("0.0.0.0/0", AF_INET, &dst); // route everything
    rtnl_route_set_dst(route, dst);

    // next-hop via your VPN server
    err = nl_addr_parse(config->vpnPrivateServerIp, AF_INET, &gateway); // VPN server IP
    if (err < 0) 
    {
        DieWithError("Invalid IP\n");
    }
    else
    {
        struct rtnl_nexthop *nh = rtnl_route_nh_alloc();
        rtnl_route_nh_set_gateway(nh, gateway);
        rtnl_route_add_nexthop(route, nh);

        // Set metric (priority)
        rtnl_route_set_priority(route, 0); // lower = preferred

        rtnl_route_add(sock, route, 0);
    }
    return err;
}

void transmitterLoop(struct vpn_context * context)
{
    ssize_t nread;
    ssize_t ndata;
    uint16_t nread_net;
    char buf[MAX_BUF_SIZE];
    char data[MAX_BUF_SIZE];
	while(1) 
	{
        nread = read(context->interfaceFd, buf, sizeof(buf));
        nread_net = htons((uint16_t)nread);

        printf("Tx %zd bytes \n", nread);

        // this is where encryption would go
        if (encryptData(buf, nread, data, &ndata, context->encryptParams) != 1)
        {
            printf("Error encrypting data\n");
            continue;
        }

        // send length header
        sendto(context->vpnSock, &nread_net, sizeof(nread_net),
            0, (struct sockaddr *)&(context->serverAddr), sizeof(context->serverAddr));

        // send actual packet
        sendto(context->vpnSock, data, ndata,
            0, (struct sockaddr *)&(context->serverAddr), sizeof(context->serverAddr));
    }
}

// takes in vpn_context struct pointer
void* spawnTransmitterThread(void* arg)
{
    printf("start Transmit--------------------\n");
    
    struct vpn_context * context = (struct vpn_context *)arg;

    transmitterLoop(context);
    
    printf("end Transmit--------------------\n");
    
    pthread_exit(NULL);
}

void recieverLoop(struct vpn_context * context)
{
    ssize_t nread;
    ssize_t ndata;
    uint16_t nread_net;
    char buf[MAX_BUF_SIZE];
    char data[MAX_BUF_SIZE];
	while(1) 
	{
        recvfrom(context->vpnSock, &nread_net, sizeof(nread_net), 0, NULL, NULL);
        nread = ntohs(nread_net);

        recvfrom(context->vpnSock, buf, nread, 0, NULL, NULL);

        printf("Rx %zd bytes \n", nread);

        // this is where decryption would go
        if (decryptData(buf, nread, data, &ndata, context->encryptParams) != 1)
        {
            printf("Error decrypting data\n");
            continue;
        }

        write(context->interfaceFd, data, ndata);
    }
}

void* spawnRecieverThread(void* arg)
{	
	printf("start Listen--------------------\n");
	
	struct vpn_context * context = (struct vpn_context *)arg;

    recieverLoop(context);

	printf("end Listen--------------------\n");
	
	pthread_exit(NULL);
}

void spawnThreads(struct vpn_context * context)
{
    pthread_t transmitter, reciever;
    pthread_create(&transmitter, NULL, &spawnTransmitterThread, context);
    pthread_create(&reciever, NULL, &spawnRecieverThread, context);

    pthread_join(transmitter, NULL);
    pthread_join(reciever, NULL);
}

int main(int argc, char *argv[])
{
    struct vpn_config config = readVPNConfig((argc < 2) ? NULL : argv[1]);

    // create shared context object
    struct vpn_context context;
    setupVPNContext(&context, config.vpnClientIp, &config, addClientRoutingRules);

    spawnThreads(&context);

    close(context.interfaceFd);

    return 0;
}