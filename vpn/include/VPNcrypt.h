#ifndef VPNCRYPT_H
#define VPNCRYPT_H
// shared code between client and server

#include <sys/types.h>

int encryptData(unsigned char * inputBuf, size_t inputLen, unsigned char * outputBuf, size_t * outputLen, const unsigned char * key, const unsigned char * iv);

int decryptData(unsigned char * inputBuf, size_t inputLen, unsigned char * outputBuf, size_t * outputLen, const unsigned char * key, const unsigned char * iv);

#endif