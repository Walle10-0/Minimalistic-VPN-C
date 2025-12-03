VPNserver.out:	VPNserver.o handleVPNclient.o
	gcc VPNserver.o handleVPNclient.o -o VPNserver.out

VPNserver.o:	VPNserver.c
	gcc -c -lpthread VPNserver.c

handleVPNclient.o:	handleVPNclient.c
	gcc -c handleVPNclient.c
