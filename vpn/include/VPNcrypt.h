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

void getCipherProperties(size_t * keyLen, size_t * tagLen, size_t * ivLen);

int encryptData(unsigned char * inputBuf, size_t inputLen, unsigned char * outputBuf, size_t * outputLen, struct encryptParams encryptParams);

int decryptData(unsigned char * inputBuf, size_t inputLen, unsigned char * outputBuf, size_t * outputLen, struct encryptParams encryptParams);

#endif