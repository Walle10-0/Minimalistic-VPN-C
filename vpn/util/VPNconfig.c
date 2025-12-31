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
    if (line[0] == EOF || line[0] == '\n')
    {
        result = 0; // empty line
    }
    else if (line[0] == '#')
    {
        result = 0; // comment
    }
    else if (sscanf(line, " interfaceName = %255s", buffer) == 1)
    {
        strncpy(config->interfaceName, buffer, IFNAMSIZ);
        config->interfaceName[IFNAMSIZ - 1] = '\0';
        result = 1;
    }
    else if (sscanf(line, " vpnClientIp = %255s", buffer) == 1)
    {
        strncpy(config->vpnClientIp, buffer, HARDCODED_IP_LENGTH);
        config->vpnClientIp[HARDCODED_IP_LENGTH - 1] = '\0';
        result = 1;
    }
    else if (sscanf(line, " vpnPublicServerIp = %255s", buffer) == 1)
    {
        strncpy(config->vpnPublicServerIp, buffer, HARDCODED_IP_LENGTH);
        config->vpnPublicServerIp[HARDCODED_IP_LENGTH - 1] = '\0';
        result = 1;
    }
    else if (sscanf(line, " vpnPrivateServerIp = %255s", buffer) == 1)
    {
        strncpy(config->vpnPrivateServerIp, buffer, HARDCODED_IP_LENGTH);
        config->vpnPrivateServerIp[HARDCODED_IP_LENGTH - 1] = '\0';
        result = 1;
    }
    else if (sscanf(line, " vpnNetwork = %255s", buffer) == 1)
    {
        strncpy(config->vpnNetwork, buffer, HARDCODED_IP_LENGTH);
        config->vpnNetwork[HARDCODED_IP_LENGTH - 1] = '\0';
        result = 1;
    }
    else if (sscanf(line, " vpnPort = %hu", &config->vpnPort) == 1)
    {
        result = 1;
    }
    else if (sscanf(line, " hardcodedKey = %255s", buffer) == 1)
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

    fclose(configFile);

    return config;
}

