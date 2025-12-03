#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TUNTAP_NAME "vpnclient"

// create TUN interface for VPN client
int createTunTap(char *name)
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
    strncpy(setIfrRequest.ifr_name, name, IFNAMSIZ);

    // create interface
    int control_error = ioctl(tunFd, TUNSETIFF, &setIfrRequest);

    if (control_error < 0) {
        // an error has occured!
        close(tunFd);
        return -1;
    }

        /* code to save the interface name
	if (iface_name_out != NULL) {
		memcpy(iface_name_out, setiff_request.ifr_name, IFNAMSIZ);
	}
        */
    print(setIfrRequest.ifr_name);
    
    return tunFd;
}

void main()
{	int len; char buf[2000];

    int tunTapFd = createTunTap(TUNTAP_NAME);

    if (tunTapFd > 0)
    {
        printf("TUN/TAP interface %s created successfully!\n", TUNTAP_NAME);
    }
    else
    {
        printf("Error creating TUN/TAP interface %s\n", TUNTAP_NAME);
        return;
    }

	while(1) 
	{
     len = read(tunTapFd, buf, sizeof(buf));

     printf("Rx %d bytes \n", len);
     len=0;
   }
   close(tunTapFd);
}