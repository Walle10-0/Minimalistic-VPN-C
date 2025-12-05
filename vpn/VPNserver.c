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

// my libraries/code
#include "VPNtools.h"
#include "VPNcrypt.h"
#include "VPNnetwork.h"
#include "VPNconfig.h"

// this TUN_OFFSET is PURE EVIL
#define TUN_OFFSET 4  // size of length header

uint32_t clientVpnIp[MAX_VPN_CLIENTS];
struct sockaddr_in clientRealIp[MAX_VPN_CLIENTS];

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

int configureIpTablesRouting(struct nl_sock *sock, char *vpnIfName, struct vpn_config * config)
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
        snprintf(cmd, sizeof(cmd), "iptables -t nat -A POSTROUTING -s %s -o %s -j MASQUERADE", config->vpnNetwork, defaultIfName);
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

int addServerRoutingRules(struct nl_sock *sock, char *vpnIfName, struct vpn_config * config)
{
    int err = 0;

    // enable forwarding and set up route
    err = enableIpForwarding();

    if (err >= 0)
    {
        // set up route forwarding via default interface
        configureIpTablesRouting(sock, vpnIfName, config);
    }

    return err;
}

struct sockaddr_in * getRealIp(char * data)
{
    // the packet contained within the buffer
    struct iphdr *ip = (struct iphdr *)(data + TUN_OFFSET);

    uint32_t vpnDest = ip->daddr;  // return traffic = dest is client

    printf("IP-dest :: %d\n", (int)vpnDest);

    for (int i=0; i<MAX_VPN_CLIENTS; i++) {
        if (clientVpnIp[i] == vpnDest) {
            //printf("gottem\n");
            return &clientRealIp[i];
        }
    }
    printf("NotFound\n");
    return NULL;
}

void transmitterLoop(struct vpn_context * context)
{
    ssize_t nread;
    ssize_t ndata;
    uint16_t nread_net;
    char buf[MAX_BUF_SIZE];
    char data[MAX_BUF_SIZE];
    struct sockaddr_in * dest_ip;

	while(1) 
	{
        nread = read(context->interfaceFd, buf, sizeof(buf));
        nread_net = htons((uint16_t)nread);

        // print the length
        printf("Tx %zd bytes \n", nread);

        dest_ip = getRealIp(buf);

        if (dest_ip == NULL)
        {
            printf("Skipping packet, no cached real IP\n");
            continue;
        }

        // this is where encryption would go
        encryptData(buf, nread, data, &ndata, HARDCODED_KEY, NULL);

        // send length header
        if (sendto(context->vpnSock, &nread_net, sizeof(nread_net),
            0, (struct sockaddr *)dest_ip, sizeof(*dest_ip)) < 0)
        {
            printf("Error sending length header\n");
            continue;
        }

        // send actual packet
        if (sendto(context->vpnSock, data, ndata,
            0, (struct sockaddr *)dest_ip, sizeof(*dest_ip)) < 0)
        {
            printf("Error sending length header\n");
            continue;
        }
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

bool cacheRealIp(struct sockaddr_in incomingClientRealIp, char * data)
{
    // the packet contained within the buffer
    struct iphdr *ip = (struct iphdr *)(data + TUN_OFFSET);

    // fill in VPN IP
    uint32_t incomingClientVpnIp = ip->saddr;
    //printf("IP-src :: %d\n", (int)incomingClientVpnIp);

    for (int i = 0; i < MAX_VPN_CLIENTS; i++)
    {
        if (clientVpnIp[i] == 0 || clientVpnIp[i] == incomingClientVpnIp)
        {
            printf("cache :: %d at %d\n", (int)incomingClientVpnIp, i);
            clientVpnIp[i] = incomingClientVpnIp;
            clientRealIp[i] = incomingClientRealIp;
            return true;
        }
    }
    printf("CacheFull\n");
    return false; // cache full
}

void recieverLoop(struct vpn_context * context)
{
    ssize_t nread;
    ssize_t ndata;
    uint16_t nread_net;
    char buf[MAX_BUF_SIZE];
    char data[MAX_BUF_SIZE];

    struct sockaddr_in incomingClientRealIp;
    socklen_t clientRealIpLen = sizeof(struct sockaddr_in);
	while(1) 
	{
        recvfrom(context->vpnSock, &nread_net, sizeof(nread_net), 0, NULL, NULL);
        nread = ntohs(nread_net);

        recvfrom(context->vpnSock, buf, nread, 0, (struct sockaddr *)&incomingClientRealIp, &clientRealIpLen);

        // print the length
        printf("Rx %zd bytes \n", nread);

        // this is where decryption would go
        decryptData(buf, nread, data, &ndata, HARDCODED_KEY, NULL);

        cacheRealIp(incomingClientRealIp, data);

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
    setupVPNContext(&context, config.vpnPrivateServerIp, &config, addServerRoutingRules);

    memset(clientVpnIp, 0, sizeof(clientVpnIp)); // initialize
    memset(clientRealIp, 0, sizeof(clientRealIp)); // initialize

    spawnThreads(&context);

    close(context.interfaceFd);
}