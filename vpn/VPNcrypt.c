#include "VPNcrypt.h"
// shared code between client and server

#include <stdint.h>
#include <stdlib.h>

// crypto headers
#include <openssl/evp.h>
#include <openssl/rand.h>

// system headers
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int encryptData(unsigned char * inputBuf, ssize_t inputLen, unsigned char * outputBuf, ssize_t * outputLen, const unsigned char * key, const unsigned char * iv)
{
    int err = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); // ig this is an object to do encryption
    int ciphertext_len;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv);
    EVP_EncryptUpdate(ctx, outputBuf, &ciphertext_len, inputBuf, inputLen);
    *outputLen = ciphertext_len;
    err = EVP_EncryptFinal_ex(ctx, outputBuf + *outputLen, &ciphertext_len);
    *outputLen += ciphertext_len;

    EVP_CIPHER_CTX_free(ctx);
    return err;
}

int decryptData(unsigned char * inputBuf, ssize_t inputLen, unsigned char * outputBuf, ssize_t * outputLen, const unsigned char * key, const unsigned char * iv)
{
    int err = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int plaintext_len;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv);
    EVP_DecryptUpdate(ctx, outputBuf, &plaintext_len, inputBuf, inputLen);
    *outputLen = plaintext_len;
    err = EVP_DecryptFinal_ex(ctx, outputBuf + *outputLen, &plaintext_len);
    *outputLen += plaintext_len;

    EVP_CIPHER_CTX_free(ctx);
    return err;
}