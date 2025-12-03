#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

// Linux TUN/TAP headers
#include <linux/if.h>
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

#define TUNTAP_NAME "vpnclient"

// create TUN interface for VPN client
int createInterface(char *interfaceName)
{
    int type = IFF_TUN; // we don't care about TAF interfaces
    int tunFd = open("/dev/net/tun", O_RDWR | O_CLOEXEC); // fd for tun interface
    struct ifreq setIfrRequest;  // interface request struct

    if (tunFd == -1) {
        // an error has occured!
		return -1;
	}

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
    
    return tunFd; // TUN interface file descriptor is the file descriptor to read our data
}

void configureInterface(char * ifName)
{
    struct nl_sock *sock = nl_socket_alloc();
    nl_connect(sock, NETLINK_ROUTE);


    struct rtnl_link *link;
    rtnl_link_get_kernel(sock, 0, "tun0", &link);

    if (!link) {
        fprintf(stderr, "Failed to allocate link for eth0.\n");
        nl_socket_free(sock);
        return 1;
    }
    
    // bring it up
    rtnl_link_set_flags(link, IFF_UP);
    rtnl_link_add(sock, link, NLM_F_CREATE);

    // add an IPv4 address
    struct rtnl_addr *addr = rtnl_addr_alloc();
    rtnl_addr_set_local(addr, inet_addr("10.8.0.2"));
    rtnl_addr_set_prefixlen(addr, 24);
    rtnl_addr_set_link(addr, rtnl_link_get_ifindex(link));
    rtnl_addr_add(sock, addr, 0);

    // cleanup
    rtnl_addr_put(addr);
    rtnl_link_put(link);
    nl_socket_free(sock);

}

void main()
{	int len; char buf[2000];

    // interface name
    char interfaceName[IFNAMSIZ];
    strncpy(interfaceName, TUNTAP_NAME, IFNAMSIZ); // we need a writable version of the name

    // create the interface and get a filedecriptor we can read and write to
    int interfaceFd = createInterface(interfaceName);

    if (interfaceFd > 0)
    {
        printf("TUN/TAP interface %s created successfully with name %s!\n", TUNTAP_NAME, interfaceName);
    }
    else
    {
        printf("Error creating TUN/TAP interface %s\n", TUNTAP_NAME);
        printf("Are you root?\n");
        return;
    }

	while(1) 
	{
     len = read(interfaceFd, buf, sizeof(buf));

     printf("Rx %d bytes \n", len);
     len=0;
   }
   close(interfaceFd);
}