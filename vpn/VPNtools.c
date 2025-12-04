#include "VPNtools.h"

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

// I should use this more often ... or not
void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

int setupUDPSocket(unsigned short fileServPort)
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

int setServerAddress(struct vpn_context * context, char * serverIP, unsigned short serverPort)
{
    // set up server address struct
    memset(&context->serverAddr, 0, sizeof(context->serverAddr));
    context->serverAddr.sin_family = AF_INET;                /* Internet address family */
    context->serverAddr.sin_port = htons(serverPort);      /* Local port */

    // Convert string IP to binary
    if (inet_pton(AF_INET, serverIP, &context->serverAddr.sin_addr) <= 0)
    {
        return -1;
    }

    return 0;
}

int autoSetServerAddress(struct vpn_context * context)
{
    return setServerAddress(context, VPN_PUBLIC_SERVER_IP, VPN_PORT);
}

// configure the TUN/TAP interface to be active with Netlink
int activateInterface(struct nl_sock *sock, struct rtnl_link *link, char * ipAddr)
{
    int err = 0;

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
        }
    }

    return err;
}

bool createNetlinkSocket(struct nl_sock ** sock, struct rtnl_link **link, char * ifName)
{
    *sock = nl_socket_alloc();
    if (*sock == NULL)
    {
        fprintf(stderr, "Failed to allocate netlink socket.\n");
        return false;
    }

    if (nl_connect(*sock, NETLINK_ROUTE) < 0)
    {
        fprintf(stderr, "Failed to connect netlink socket.\n");
        nl_socket_free(*sock);
        return false;
    }

    rtnl_link_get_kernel(*sock, 0, ifName, link);

    if (!*link)
    {
        fprintf(stderr, "Failed to allocate link for eth0.\n");
        return false;
    }

    return true;
}

// configure the TUN/TAP interface with Netlink
int configureInterface(char * ifName, char * ipAddr, int (*f)(struct nl_sock *) specialConfiguration)
{
    int err = 0;
    struct nl_sock *sock;
    struct rtnl_link *link;

    createNetlinkSocket(&sock, &link, ifName);

    if (!createNetlinkSocket(&sock, &link, ifName))
    {
        fprintf(stderr, "Failed to create netlink socket\n");
        err = -1;
    }
    else
    {
        err = activateInterface(sock, link, ipAddr);
        if (err >= 0 && specialConfiguration != NULL)
        {
            err = specialConfiguration(sock);
            if (err < 0)
            {
                fprintf(stderr, "Error in special configuration\n");
            }
        }
    }
    nl_socket_free(sock);

    return err;
}

// create TUN interface for VPN
int createInterface(char *interfaceName, char * ipAddr, int (*f)(struct nl_sock *) specialConfiguration)
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

        if (configureInterface(interfaceName, ipAddr, specialConfiguration) < 0)
        {
            fprintf(stderr, "Error in connfiguring interface\n");
            close(tunFd);
            return -1;
        }
	}


    return tunFd; // TUN interface file descriptor is the file descriptor to read our data
}
*/