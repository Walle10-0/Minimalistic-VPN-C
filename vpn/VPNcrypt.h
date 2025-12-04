#ifndef VPNCRYPT_H
#define VPNCRYPT_H
// shared code between client and server

// VPN config constants
#define HARDCODED_KEY "1e27bbe48a548a9cd4ab0b86b04bb8c4220d3e179d45e07a90f3467e63d15b8c"

int encryptData(unsigned char * inputBuf, ssize_t inputLen, unsigned char * outputBuf, ssize_t * outputLen, const unsigned char * key, const unsigned char * iv);

int decryptData(unsigned char * inputBuf, ssize_t inputLen, unsigned char * outputBuf, ssize_t * outputLen, const unsigned char * key, const unsigned char * iv);

#endif