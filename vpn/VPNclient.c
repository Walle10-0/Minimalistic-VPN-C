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

int addRoutingRules(struct nl_sock *sock)
{
    int err = 0;
    // re-route all traffic through the VPN
    struct rtnl_route *route = rtnl_route_alloc();
    struct nl_addr *dst, *gateway;

    // default route
    nl_addr_parse("0.0.0.0/0", AF_INET, &dst); // route everything
    rtnl_route_set_dst(route, dst);

    // next-hop via your VPN server
    err = nl_addr_parse(VPN_PRIVATE_SERVER_IP, AF_INET, &gateway); // VPN server IP
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

void setupVPNContext(struct vpn_context * context)
{
    // zero it out
    memset(context, 0, sizeof(context));

    // interface name
    char interfaceName[IFNAMSIZ];
    strncpy(interfaceName, TUNTAP_NAME, IFNAMSIZ); // we need a writable version of the name

    // create the interface and get a filedecriptor we can read and write to
    context->interfaceFd = createInterface(interfaceName, VPN_CLIENT_IP, addRoutingRules);

    // check for error
    if (context->interfaceFd  > 0)
    {
        printf("TUN/TAP interface %s created successfully with name %s!\n", TUNTAP_NAME, interfaceName);
    }
    else
    {
        printf("Error creating TUN/TAP interface %s\n", TUNTAP_NAME);
        DieWithError("Are you root?\n");
    }
    
    // setup VPN socket
    context->vpnSock = setupUDPSocket(VPN_PORT); // hardcoded port for now

    // check for error
    if (context->vpnSock  > 0)
    {
        printf("VPN socket created successfully!\n");
    }
    else
    {
        close(context->interfaceFd);
        DieWithError("Error creating VPN socket\n");
    }

    // set up server address struct
    if (autoSetServerAddress(context) <= 0)
    {
        DieWithError("inet_pton failed");
    }

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
    setupVPNContext(&context);

    spawnThreads(&context);

    close(context.interfaceFd);
}