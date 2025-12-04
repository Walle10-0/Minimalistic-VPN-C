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

#include <net/if.h>
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

#include "VPNtools.h"

char *getDefaultInterface(struct nl_sock *sock)
{
    struct rtnl_route *route;
    struct nl_cache *cache;
    struct rtnl_link *link;
    char *ifName = NULL;

    rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache);

    // iterate all routes
    rtnl_route_alloc(); // create route object
    // better: use rtnl_route_get_default() if your libnl version supports it

    // simplified: you can run `ip route show default` via popen and parse the line
    // e.g.,
    // default via 192.168.1.1 dev enp1s0 proto dhcp metric 100

    return ifName; // you would strdup() it
}

int enableIpForwarding()
{
    int fd = open("/proc/sys/net/ipv4/ip_forward", O_WRONLY);
    int err = fd;
    if (err >= 0)
    {
        err = write(fd, "1", 1);
        close(fd);
    }
    return err;
}

int configureIpTablesRouting(struct nl_sock *sock, char *vpnIfName)
{
    // set up route forwarding via default interface
    char *defaultIfName = getDefaultInterface(sock); // this doesn't work rn lol

    char cmd[256];
    
    //iptables -t nat -A POSTROUTING -o <out-if> -j MASQUERADE
    snprintf(cmd, sizeof(cmd), "iptables -t nat -A POSTROUTING -o %s-j MASQUERADE", defaultIfName);
    system(cmd);

    //iptables -A FORWARD -i vpnserver -o <out-if> -j ACCEPT
    snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -i %s -o %s -j ACCEPT", vpnIfName, defaultIfName);
    system(cmd);

    //iptables -A FORWARD -i <out-if> -o vpnserver -m state --state RELATED,ESTABLISHED -j ACCEPT
    snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -i %s-o %s -m state --state RELATED,ESTABLISHED -j ACCEPT", defaultIfName, vpnIfName);
    system(cmd);
}

int addServerRoutingRules(struct nl_sock *sock, char *vpnIfName)
{
    int err = 0;

    // enable forwarding and set up route
    err = enableIpForwarding();

    if (err >= 0)
    {
        // set up route forwarding via default interface
        configureIpTablesRouting(sock, vpnIfName);
    }

    return err;
}

void transmitterLoop(struct vpn_context * context)
{
    ssize_t nread;
    uint16_t nread_net;
    char buf[MAX_BUF_SIZE];
	while(1) 
	{
        nread = read(context->interfaceFd, buf, sizeof(buf));
        nread_net = htons((uint16_t)nread);

        printf("Tx %zd bytes \n", nread);

        // this is where encryption would go

        // send length header
        sendto(context->vpnSock, &nread_net, sizeof(nread_net),
            0, (struct sockaddr *)&(context->serverAddr), sizeof(context->serverAddr));

        // send actual packet
        sendto(context->vpnSock, buf, nread,
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
    uint16_t nread_net;
    char buf[MAX_BUF_SIZE];
	while(1) 
	{
        recvfrom(context->vpnSock, &nread_net, sizeof(nread_net), 0, NULL, NULL);
        nread = ntohs(nread_net);

        recvfrom(context->vpnSock, buf, nread, 0, NULL, NULL);

        printf("Rx %zd bytes \n", nread);

        // this is where decryption would go

        write(context->interfaceFd, buf, nread);
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

void main()
{
    // create shared context object
    struct vpn_context context;
    setupVPNContext(&context, VPN_CLIENT_IP, addServerRoutingRules);

    spawnThreads(&context);

    close(context.interfaceFd);
}