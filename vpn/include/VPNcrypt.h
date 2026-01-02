#ifndef VPNCRYPT_H
#define VPNCRYPT_H
// shared code between client and server

#include <sys/types.h>

struct encrypted_payload {
    size_t data_len;
    unsigned char * iv;
    unsigned char * data;
    unsigned char * tag;
};

int encryptData(unsigned char * inputBuf, size_t inputLen, unsigned char * outputBuf, size_t * outputLen, const unsigned char * key, const unsigned char * iv);

int decryptData(unsigned char * inputBuf, size_t inputLen, unsigned char * outputBuf, size_t * outputLen, const unsigned char * key, const unsigned char * iv);

#endif