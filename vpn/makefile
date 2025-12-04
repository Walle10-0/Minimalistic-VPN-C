CFLAGS = -I/usr/include/libnl3
LDFLAGS = -lnl-3 -lnl-genl-3 -lnl-route-3 -lpthread -lcrypto

all: client server clean

client: VPNclient.o VPNtools.o
	gcc VPNclient.o VPNtools.o -o VPNclient.out $(LDFLAGS)

VPNclient.o: VPNclient.c
	gcc $(CFLAGS) -c VPNclient.c

server: VPNserver.o VPNtools.o
	gcc VPNserver.o VPNtools.o -o VPNserver.out $(LDFLAGS)

VPNserver.o: VPNserver.c
	gcc $(CFLAGS) -c VPNserver.c

VPNtools.o: VPNtools.c
	gcc $(CFLAGS) -c VPNtools.c

clean:
	rm -f *.o

purge:
	rm -f *.o *.out