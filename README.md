# Minimalistic-VPN-C

A minimalist VPN made in C as a class project and follow up to my minimalist HTTP Server in C

took the basic code outline from my [minimalist HTTP Server in C](https://github.com/Walle10-0/Minimalistic-HTTP-Server.git)

currently this is unfinished

### Features  
 - reads from config file  
 - creates TUN interface on client and server  
 - sets TUN interface as default route on client
 - sets up routing using TUN interface on server
 - encapsulates packets in UDP packets between client and server  
 - enrypts contents with aes_256 using key from config file
 - server routes traffic encrypted traffic from client and is able to route it back to the client
 - the client can ping the internal virtual IP addresses defined by the VPN (eg. the VPN's internal IP)

### Missing
 - handshake using public key encryption to create a per session symmetric key sent via TCP connection
 - initialization vector sent via TCP connection at least once
 - memory cleanup/security (error handling is a mess and I need to make sure memory is freed)

## Dependencies

built for Linux

requires libnl3 to build

```bash
sudo apt-get install libnl-3-dev libnl-genl-3-dev libnl-route-3-dev
```

server requires iptables to run

```bash
whereis iptables
```

you can install it with

```bash
sudo apt update
sudo apt install iptables
```

## How to Build

go into `./vpn/`

```bash
cd vpn
```

build server with

```bash
make server
```

build client with

```bash
make client
```

or build both with

```bash
make
```

## Usage

run server with

```bash
bin/VPNserver.out [vpnconfig.cfg]
```

or client with

```bash
bin/VPNclient.out [vpnconfig.cfg]
```

where `[vpnconfig.cfg]` is the path to your config file

the default config file is `./vpnconfig.cfg`

