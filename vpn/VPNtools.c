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
    nl_socket_free(sock);

    return err;
}
