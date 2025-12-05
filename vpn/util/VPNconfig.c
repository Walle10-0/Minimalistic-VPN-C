#include "VPNconfig.h"

// imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/if.h>

// shared code to read config files for VPN

#define CONFIG_BUFFER 256

int parseConfigLine(char * line, struct vpn_config * config)
{
    char buffer[CONFIG_BUFFER];
    int result = 0;

    // long else if ladder
    if (sscanf(line, " %s", buffer) == EOF)
    {
        result = 0; // empty line
    }
    else if (sscanf(line, " %[#]", buffer) == 1)
    {
        result = 0; // comment
    }
    else if (sscanf(line, " interfaceName = %s", buffer) == 1)
    {
        strncpy(config->interfaceName, buffer, IFNAMSIZ);
        result = 1;
    }
    else if (sscanf(line, " vpnClientIp = %s", buffer) == 1)
    {
        strncpy(config->vpnClientIp, buffer, HARDCODED_IP_LENGTH);
        result = 1;
    }
    else if (sscanf(line, " vpnPublicServerIp = %s", buffer) == 1)
    {
        strncpy(config->vpnPublicServerIp, buffer, HARDCODED_IP_LENGTH);
        result = 1;
    }
    else if (sscanf(line, " vpnPrivateServerIp = %s", buffer) == 1)
    {
        strncpy(config->vpnPrivateServerIp, buffer, HARDCODED_IP_LENGTH);
        result = 1;
    }
    else if (sscanf(line, " vpnPrivateServerIp = %s", buffer) == 1)
    {
        strncpy(config->vpnPrivateServerIp, buffer, HARDCODED_IP_LENGTH);
        result = 1;
    }
    else if (sscanf(line, " vpnNetwork = %s", buffer) == 1)
    {
        strncpy(config->vpnNetwork, buffer, HARDCODED_IP_LENGTH);
        result = 1;
    }
    else if (sscanf(line, " vpnPort = %hu", &config->vpnPort) == 1)
    {
        result = 1;
    }
    else if (sscanf(line, " hardcodedKey = %s", buffer) == 1)
    {
        // allocate memory for the key
        size_t keyLen = strlen(buffer);
        config->hardcodedKey = (unsigned char *)malloc(keyLen + 1); // this is messy and will come back to haunt me
        strncpy((char *)config->hardcodedKey, buffer, keyLen + 1);
    }
    else
    {
        result = -1; // invalid line
    }

    return result;
}

struct vpn_config readVPNConfig(char * configFilePath)
{
    struct vpn_config config;

    memset(&config, 0, sizeof(config));

    if (configFilePath == NULL)
    {
        configFilePath = DEFAULT_CONFIG_FILE;
    }

    FILE * configFile = fopen(configFilePath, "r"); 
    char buffer[CONFIG_BUFFER];
    int err = 0;

    while(fgets(buffer, CONFIG_BUFFER, configFile) != NULL)
    {
        err = parseConfigLine(buffer, &config);
        if (err < 0)
        {
            fprintf(stderr, "Error parsing config line: %s\n", buffer);
        }
    }

    return config;
}

