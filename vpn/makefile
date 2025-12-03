CFLAGS = -I/usr/include/libnl3
LDFLAGS = -lnl-3 -lnl-genl-3 -lnl-route-3 -lpthread

VPNclient.out: VPNclient.o
	gcc VPNclient.o -o VPNclient.out $(LDFLAGS)

VPNclient.o: VPNclient.c
	gcc $(CFLAGS) -c VPNclient.c
