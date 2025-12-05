# Minimalistic-VPN-C

A minimalist VPN made in C as a class project and follow up to my minimalist HTTP Server in C

took the basic code outline from my [minimalist HTTP Server in C](https://github.com/Walle10-0/Minimalistic-HTTP-Server.git)

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

`cd vpn`

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

`bin/VPNserver.out [vpnconfig.cfg]`

or client with

`bin/VPNclient.out [vpnconfig.cfg]`

where `[vpnconfig.cfg]` is the path to your config file

the default config file is `./vpnconfig.cfg`

## Notes

Error handling is a mess