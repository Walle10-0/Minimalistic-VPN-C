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

char *getDefaultInterface()
{
    struct nl_sock *sock = nl_socket_alloc();
    nl_connect(sock, NETLINK_ROUTE);

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

    nl_socket_free(sock);
    return ifName; // you would strdup() it
}

// configure the TUN/TAP interface with Netlink
int configureInterface(char * ifName)
{
    int err = 0;
    struct nl_sock *sock = nl_socket_alloc();
    nl_connect(sock, NETLINK_ROUTE);


    struct rtnl_link *link;
    rtnl_link_get_kernel(sock, 0, ifName, &link);

    if (!link)
    {
        fprintf(stderr, "Failed to allocate link for eth0.\n");
        err = -1;
    }
    else
    {
        // bring up interface
        unsigned int flags = rtnl_link_get_flags(link);
        flags |= IFF_UP;
        rtnl_link_set_flags(link, flags);

        // apply changes to existing interface
        rtnl_link_change(sock, link, link, 0);
        
        // add an IPv4 address
        struct rtnl_addr *addr = rtnl_addr_alloc();

        struct nl_addr *local;

        err = nl_addr_parse(VPN_PRIVATE_SERVER_IP, AF_INET, &local); // VPN client IP
        if (err < 0)
        {
            fprintf(stderr, "Invalid IP\n");
        }
        else
        {
            rtnl_addr_set_local(addr, local);
            rtnl_addr_set_link(addr, link);
        
            err = rtnl_addr_add(sock, addr, 0);
            if (err < 0) {
                fprintf(stderr, "Failed to add address: %s\n", nl_geterror(err));
            }
            else
            {
                // cleanup
                rtnl_addr_put(addr);
                rtnl_link_put(link);

                // enable forwarding and set up route
                system("sysctl -w net.ipv4.ip_forward=1");

            }
        }
    }
    nl_socket_free(sock);

    return err;
}

// create TUN interface for VPN client
int createInterface(char *interfaceName)
{
    int type = IFF_TUN; // we don't care about TAF interfaces
    int tunFd = open("/dev/net/tun", O_RDWR | O_CLOEXEC); // fd for tun interface
    struct ifreq setIfrRequest;  // interface request struct

    if (tunFd != -1) {
        // zero out the ifreq struct
        memset(&setIfrRequest, 0, sizeof(setIfrRequest));

        // set flags
        setIfrRequest.ifr_flags = IFF_TUN; // we don't care about TAP interfaces

        // set name
        strncpy(setIfrRequest.ifr_name, interfaceName, IFNAMSIZ);

        // create interface
        int control_error = ioctl(tunFd, TUNSETIFF, &setIfrRequest);

        if (control_error < 0) {
            // an error has occured!
            close(tunFd);
            return -1;
        }

        // update interface name incase it's different than requested
        // NOTE: this changes the name variable in the caller as well
        memcpy(interfaceName, setIfrRequest.ifr_name, IFNAMSIZ);

        if (configureInterface(interfaceName) < 0)
        {
            fprintf(stderr, "Error in connfiguring interface\n");
            close(tunFd);
            return -1;
        }
	}


    return tunFd; // TUN interface file descriptor is the file descriptor to read our data
}

void setupVPNContext(struct vpn_context * context)
{
    // zero it out
    memset(context, 0, sizeof(context));

    // interface name
    char interfaceName[IFNAMSIZ];
    strncpy(interfaceName, TUNTAP_NAME, IFNAMSIZ); // we need a writable version of the name

    // create the interface and get a filedecriptor we can read and write to
    context->interfaceFd = createInterface(interfaceName);

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
    context->vpnSock = setupSocket(VPN_PORT); // hardcoded port for now

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
    memset(&context->serverAddr, 0, sizeof(context->serverAddr));
    context->serverAddr.sin_family = AF_INET;                /* Internet address family */
	context->serverAddr.sin_port = htons(VPN_PORT);      /* Local port */

    // Convert string IP to binary
    if (inet_pton(AF_INET, VPN_PUBLIC_SERVER_IP, &context->serverAddr.sin_addr) <= 0)
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