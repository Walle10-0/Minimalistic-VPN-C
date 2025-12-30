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

#define CIPHER "AES-256-GCM"

int encryptData(unsigned char * inputBuf, size_t inputLen, unsigned char * outputBuf, size_t * outputLen, const unsigned char * key, const unsigned char * iv)
{
    unsigned char tag[16]; // placeholder for later tag use
    int ciphertext_len;

    // ig this is an object to do encryption
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
    {
        // Handle error: context creation failed
        return -1; // or some other error code
    }

    // fetch the cipher engine
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, CIPHER, NULL);
    if (cipher == NULL)
    {
        // Handle error: cipher not found
        EVP_CIPHER_CTX_free(ctx);
        return -1; // or some other error code
    }

    // initialize the encryption object
    if (EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) != 1)
    {
        // Handle error: initialization failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }

    // actually perform the encryption
    if (EVP_EncryptUpdate(ctx, outputBuf, &ciphertext_len, inputBuf, inputLen) != 1)
    {
        // Handle error: encryption update failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }
    *outputLen = ciphertext_len;

    // finalize the encryption
    if (EVP_EncryptFinal_ex(ctx, outputBuf + *outputLen, &ciphertext_len) != 1)
    {
        // Handle error: encryption finalization failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }

    *outputLen += ciphertext_len;

    /*
    // get the tag
    int tag_len = EVP_CIPHER_get_tag_length(cipher);
    if (tag_len > 0)
    {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tag_len, tag) != 1)
        {
            // Handle error: getting tag failed
            EVP_CIPHER_CTX_free(ctx);
            EVP_CIPHER_free(cipher);
            return -1; // or some other error code
        }
    }
    */
    
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    return 1;
}

int decryptData(unsigned char * inputBuf, ssize_t inputLen, unsigned char * outputBuf, ssize_t * outputLen, const unsigned char * key, const unsigned char * iv)
{
    unsigned char tag[16]; // placeholder for later tag use
    int plaintext_len;

    // ig this is an object to do encryption
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
    {
        // Handle error: context creation failed
        return -1; // or some other error code
    }

    // fetch the cipher engine
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, CIPHER, NULL);
    if (cipher == NULL)
    {
        // Handle error: cipher not found
        EVP_CIPHER_CTX_free(ctx);
        return -1; // or some other error code
    }

    // initialize the decryption object
    if (EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) != 1)
    {
        // Handle error: initialization failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }

    //actually perform the decryption
    if (EVP_DecryptUpdate(ctx, outputBuf, &plaintext_len, inputBuf, inputLen) != 1)
    {
        // Handle error: encryption update failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }
    *outputLen = plaintext_len;

    /*
    // set the tag
    int tag_len = EVP_CIPHER_get_tag_length(cipher);
    if (tag_len > 0)
    {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, tag_len, tag) != 1)
        {
            // Handle error: getting tag failed
            EVP_CIPHER_CTX_free(ctx);
            EVP_CIPHER_free(cipher);
            return -1; // or some other error code
        }
    }
    */

    // finalize the decryption
    if (EVP_DecryptFinal_ex(ctx, outputBuf + *outputLen, &plaintext_len) != 1)
    {
        // Handle error: encryption finalization failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }
    *outputLen += plaintext_len;

    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    return 1;
}