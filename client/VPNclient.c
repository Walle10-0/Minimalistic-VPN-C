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

#define TUNTAP_NAME "vpnclient"

#define VPN_CLIENT_IP "10.8.0.2/24"
#define VPN_SERVER_IP "10.8.0.1"
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
void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
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

        err = nl_addr_parse(VPN_CLIENT_IP, AF_INET, &local); // VPN client IP
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
                // re-route all traffic through the VPN
                struct rtnl_route *route = rtnl_route_alloc();
                struct nl_addr *dst, *gateway;

                // default route
                nl_addr_parse("0.0.0.0/0", AF_INET, &dst); // route everything
                rtnl_route_set_dst(route, dst);

                // next-hop via your VPN server
                err = nl_addr_parse(VPN_SERVER_IP, AF_INET, &gateway); // VPN server IP
                if (err < 0) 
                {
                    fprintf(stderr, "Invalid IP\n");
                }
                else
                {
                    struct rtnl_nexthop *nh = rtnl_route_nh_alloc();
                    rtnl_route_nh_set_gateway(nh, gateway);
                    rtnl_route_add_nexthop(route, nh);

                    // Set metric (priority)
                    rtnl_route_set_priority(route, 0); // lower = preferred

                    rtnl_route_add(sock, route, 0);

                    // cleanup
                    rtnl_addr_put(addr);
                    rtnl_link_put(link);
                }
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

int setupSocket(unsigned short fileServPort)
{
	int servSockAddr;
	struct sockaddr_in localServAddr; // Local address

	/* Create socket for incoming connections */
	// note SOCK_DGRAM for UDP not SOCK_STREAM for TCP
	if ((servSockAddr = socket(PF_INET, SOCK_DGRAM, 0/*IPPROTO_UDP*/)) < 0)
		DieWithError("socket() failed");
  
	/* Construct local address structure */
	memset(&localServAddr, 0, sizeof(localServAddr));   /* Zero out structure */
	localServAddr.sin_family = AF_INET;                /* Internet address family */
	localServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
	localServAddr.sin_port = htons(fileServPort);      /* Local port */

	/* Bind to the local address */
	if (bind(servSockAddr, (struct sockaddr *) &localServAddr, sizeof(localServAddr)) < 0)
		DieWithError("bind() failed");

	return servSockAddr;
}

void setupVPNContext(struct vpn_context * context)
{
    // zero it out
    memset(&context, 0, sizeof(context));

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
    if (inet_pton(AF_INET, VPN_SERVER_IP, &context->serverAddr.sin_addr) <= 0)
    {
        perror("inet_pton failed");
        exit(1);
    }

}

// takes in vpn_context struct pointer
void* spawnTransmitterThread(void* arg)
{
    printf("start Transmit--------------------\n");
    
    struct vpn_context * context = (struct vpn_context *)arg;

    ssize_t nread;
    uint16_t nread_net;
    char buf[MAX_BUF_SIZE];
	while(1) 
	{
        nread = read(context->interfaceFd, buf, sizeof(buf));
        nread_net = htons((uint16_t)nread);

        printf("Tx %d bytes \n", nread);

        // send length header
        sendto(context->vpnSock, &nread_net, sizeof(nread_net),
            0, &(context->serverAddr), sizeof(context->serverAddr));

        // send actual packet
        sendto(context->vpnSock, buf, nread,
            0, &(context->serverAddr), sizeof(context->serverAddr));
    }
    
    printf("end Transmit--------------------\n");
    
    pthread_exit(NULL);
}

void* spawnRecieverThread(void* arg)
{	
	printf("start Listen--------------------\n");
	
	struct vpn_context * context = (struct vpn_context *)arg;

    ssize_t nread;
    uint16_t nread_net;
    char buf[MAX_BUF_SIZE];
	while(1) 
	{
        recvfrom(context->vpnSock, &nread_net, sizeof(nread_net), 0, NULL, NULL);
        nread = ntohs(nread_net);

        recvfrom(context->vpnSock, buf, nread, 0, NULL, NULL);

        printf("Rx %d bytes \n", nread);

        write(context->interfaceFd, buf, nread);
    }
	
	printf("end Listen--------------------\n");
	
	pthread_exit(NULL);
}

void main()
{
    // create shared context object
    struct vpn_context context;
    setupVPNContext(&context);

    pthread_t transmitter, reciever;
    pthread_create(&transmitter, NULL, &spawnTransmitterThread, &context);
    pthread_create(&reciever, NULL, &spawnRecieverThread, &context);

    pthread_join(transmitter, NULL);
    pthread_join(reciever, NULL);


   close(context.interfaceFd);
}