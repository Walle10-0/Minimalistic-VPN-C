#include "VPNcrypt.h"
// shared code between client and server

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// crypto headers
#include <openssl/evp.h>
#include <openssl/rand.h>

// system headers
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "VPNconfig.h"

#define CIPHER "AES-256-GCM" //"AES-256-CTR"

void getCipherProperties(size_t * keyLen, size_t * tagLen, size_t * ivLen)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, CIPHER, NULL);
    if (cipher != NULL)
    {
        if (keyLen)
            *keyLen = EVP_CIPHER_get_key_length(cipher);
        if (tagLen)
        {
            EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
            if (ctx != NULL)
            {
                if (EVP_EncryptInit_ex2(ctx, cipher, NULL, NULL, NULL) == 1)
                {
                    *tagLen = EVP_CIPHER_CTX_get_tag_length(ctx);
                }
                EVP_CIPHER_CTX_free(ctx);
            }
        } 
        if (ivLen)
            *ivLen = EVP_CIPHER_get_iv_length(cipher);
        EVP_CIPHER_free(cipher);
    }

}

int encryptData(unsigned char * inputBuf, size_t inputLen, unsigned char * outputBuf, size_t * outputLen, struct encryptParams encryptParams)
{
    unsigned char tag[encryptParams.tag_len];
    unsigned char iv[encryptParams.iv_len];
    int ciphertext_len;

    *outputLen = 0;
    memset(tag, 0, encryptParams.tag_len);

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

    // generate IV (random for now)
    if (RAND_bytes(iv, encryptParams.iv_len) != 1)
    {
        // Handle error: IV generation failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, encryptParams.iv_len, NULL);
    printf("IV len: %d\n", encryptParams.iv_len);

    printf("IV: ");
    for (size_t i = 0; i < encryptParams.iv_len; i++)
    {
        printf("%02x", iv[i]);
    }
    printf("\n");

    // prepend IV to output
    memcpy(outputBuf + *outputLen, iv, encryptParams.iv_len);
    *outputLen += encryptParams.iv_len;

    // initialize the encryption object
    if (EVP_EncryptInit_ex2(ctx, cipher, encryptParams.key, iv, NULL) != 1)
    {
        // Handle error: initialization failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }

    // actually perform the encryption
    if (EVP_EncryptUpdate(ctx, outputBuf + *outputLen, &ciphertext_len, inputBuf, inputLen) != 1)
    {
        // Handle error: encryption update failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }
    *outputLen += ciphertext_len;

    // finalize the encryption
    if (EVP_EncryptFinal_ex(ctx, outputBuf + *outputLen, &ciphertext_len) != 1)
    {
        // Handle error: encryption finalization failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }

    *outputLen += ciphertext_len;

    // get the tag
    if (encryptParams.tag_len > 0)
    {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, encryptParams.tag_len, tag) != 1)
        {
            printf("Failed to get tag\n");
            // Handle error: getting tag failed
            EVP_CIPHER_CTX_free(ctx);
            EVP_CIPHER_free(cipher);
            return -1; // or some other error code
        }

        // append tag to output
        memcpy(outputBuf + *outputLen, tag, encryptParams.tag_len);
        *outputLen += encryptParams.tag_len;
    }


    
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    return 1;
}

int decryptData(unsigned char * inputBuf, size_t inputLen, unsigned char * outputBuf, size_t * outputLen, struct encryptParams encryptParams)
{
    struct encrypted_payload payload;
    int plaintext_len;
    *outputLen = 0;

    // get fields from payload
    payload.iv = inputBuf;
    payload.data = inputBuf + encryptParams.iv_len;
    payload.tag = inputBuf + inputLen - encryptParams.tag_len;
    payload.data_len = inputLen - encryptParams.iv_len - encryptParams.tag_len;

    // ig this is an object to do encryption
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
    {
        // Handle error: context creation failed
        return -1; // or some other error code
    }

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, encryptParams.iv_len, NULL);
    printf("IV len: %d\n", encryptParams.iv_len);

    printf("IV: ");
    for (size_t i = 0; i < encryptParams.iv_len; i++)
    {
        printf("%02x", payload.iv[i]);
    }
    printf("\n");

    // fetch the cipher engine
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, CIPHER, NULL);
    if (cipher == NULL)
    {
        // Handle error: cipher not found
        EVP_CIPHER_CTX_free(ctx);
        return -1; // or some other error code
    }

    // initialize the decryption object
    if (EVP_DecryptInit_ex2(ctx, cipher, encryptParams.key, payload.iv, NULL) != 1)
    {
        printf("Failed init\n");
        // Handle error: initialization failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }

    //actually perform the decryption
    if (EVP_DecryptUpdate(ctx, outputBuf, &plaintext_len, payload.data, payload.data_len) != 1)
    {
        printf("Failed encryption\n");
        // Handle error: encryption update failed
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        return -1; // or some other error code
    }
    *outputLen = plaintext_len;

    // set the tag
    if (encryptParams.tag_len > 0)
    {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, encryptParams.tag_len, payload.tag) != 1)
        {
            printf("Failed to get tag\n");
            // Handle error: getting tag failed
            EVP_CIPHER_CTX_free(ctx);
            EVP_CIPHER_free(cipher);
            return -1; // or some other error code
        }
    }

    // finalize the decryption
    if (EVP_DecryptFinal_ex(ctx, outputBuf + *outputLen, &plaintext_len) != 1)
    {
        printf("Failed final encryption\n");
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