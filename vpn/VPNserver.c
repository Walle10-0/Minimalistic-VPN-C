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
#include <netlink/cache.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/ip.h>

// system headers
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>  // for multi-threading

#include "VPNtools.h"

char *getDefaultInterface(struct nl_sock *sock)
{
    struct nl_cache *route_cache = NULL;
    if (rtnl_route_alloc_cache(sock, AF_INET, 0, &route_cache) < 0) {
        nl_socket_free(sock);
        return NULL;
    }

    struct nl_object *obj;
    char *result = NULL;

    // iterate over all routes
    for (obj = nl_cache_get_first(route_cache);
         obj != NULL;
         obj = nl_cache_get_next(obj))
    {
        struct rtnl_route *route = (struct rtnl_route*)obj;
        struct nl_addr *dst = rtnl_route_get_dst(route);
        // For the default route, the destination (dst) should be NULL or 0.0.0.0/0
        if (rtnl_route_get_family(route) == AF_INET &&
            (!dst || nl_addr_get_prefixlen(dst) == 0))
        {
            struct rtnl_nexthop *nh = rtnl_route_nexthop_n(route, 0);
            if (!nh) continue;
            int ifidx = rtnl_route_nh_get_ifindex(nh);

            char ifName[IFNAMSIZ];
            if (if_indextoname(ifidx, ifName)) {
                result = strdup(ifName);  // allocate copy for caller
                // NOTE: caller must free() later on
            }
            break;
        }
    }

    nl_cache_free(route_cache);
    return result;  // caller must free()
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

    if (!defaultIfName)
    {
        DieWithError("Could not get default interface name\n");
    }
    else
    {
        printf("Default interface is %s\n", defaultIfName);

        char cmd[256];
    
        //iptables -t nat -A POSTROUTING -o <out-if> -j MASQUERADE
        snprintf(cmd, sizeof(cmd), "iptables -t nat -A POSTROUTING -s %s -o %s -j MASQUERADE", VPN_NETWORK, defaultIfName);
        system(cmd);

        //iptables -A FORWARD -i vpnserver -o <out-if> -j ACCEPT
        snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -i %s -o %s -j ACCEPT", vpnIfName, defaultIfName);
        system(cmd);

        //iptables -A FORWARD -i <out-if> -o vpnserver -m state --state RELATED,ESTABLISHED -j ACCEPT
        snprintf(cmd, sizeof(cmd), "iptables -A FORWARD -i %s -o %s -m state --state RELATED,ESTABLISHED -j ACCEPT", defaultIfName, vpnIfName);
        system(cmd);

        // where does this go?
        //iptables -t nat -A POSTROUTING -s 10.8.0.0/24 -o enp1s0 -j MASQUERADE

        free(defaultIfName);
    }
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
    struct sockaddr_in dest_ip;

    memset(&dest_ip, 0, sizeof(dest_ip));
    dest_ip.sin_family = AF_INET;           // IPv4
    inet_pton(AF_INET, "10.8.0.2", &dest_ip.sin_addr);
	while(1) 
	{
        nread = read(context->interfaceFd, buf, sizeof(buf));
        nread_net = htons((uint16_t)nread);

        // the packet contained within the buffer
        struct iphdr *ip = (struct iphdr *)buf;

        // extract destination IP address
        //memset(&dest_ip, 0, sizeof(dest_ip));
        //dest_ip.sin_family = AF_INET;           // IPv4
        //dest_ip.sin_addr.s_addr = ip->daddr;

        printf("Tx %zd bytes \n", nread);

        // this is where encryption would go

        // send length header
        sendto(context->vpnSock, &nread_net, sizeof(nread_net),
            0, (struct sockaddr *)&dest_ip, sizeof(dest_ip));

        // send actual packet
        sendto(context->vpnSock, buf, nread,
            0, (struct sockaddr *)&dest_ip, sizeof(dest_ip));

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
    setupVPNContext(&context, VPN_PRIVATE_SERVER_IP, addServerRoutingRules);

    spawnThreads(&context);

    close(context.interfaceFd);
}