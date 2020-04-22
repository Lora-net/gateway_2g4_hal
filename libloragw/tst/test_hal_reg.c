/*!
 * License: Revised BSD 3-Clause License, see LICENSE.TXT file include in the project
 */

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf */
#include <stdlib.h>     /* EXIT_FAILURE */
#include <getopt.h>     /* getopt_long */

#include "loragw_aux.h"
#include "loragw_mcu.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define TTY_PATH_DEFAULT "/dev/ttyACM0"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

/* describe command line options */
void usage(void) {
    printf("Library version information: %s\n", lgw_version_info());
    printf("Available options:\n");
    printf(" -h print this help\n");
    printf(" -d <path>  TTY device to be used to access the concentrator board\n");
    printf("                      => default path: " TTY_PATH_DEFAULT "\n");
    printf(" -r <uint>  Radio index from which read/write register [0..3]\n");
    printf(" -a <uint>  Radio register address (hexadecimal) from which read/write register\n");
    printf(" -v <uint>  Value (hexadecimal) to be written in radio register\n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv) {
    int i, x;
    int fd;
    uint8_t reg_val;
    unsigned int arg_u;
    struct lgw_conf_channel_rx_s conf;
    s_ping_info gw_info;
    uint8_t radio_idx = 0;
    uint16_t reg_addr = 0x8C1;
    uint8_t reg_val_wr = 0xAA;

    /* TTY interfaces */
    const char tty_path_default[] = TTY_PATH_DEFAULT;
    const char * tty_path = tty_path_default;

    /* Parameter parsing */
    int option_index = 0;
    static struct option long_options[] = {
        {0, 0, 0, 0}
    };

    /* parse command line options */
    while ((i = getopt_long (argc, argv, "hd:r:a:v:", long_options, &option_index)) != -1) {
        switch (i) {
            case 'h':
                usage();
                return -1;
                break;
            case 'd':
                if (optarg != NULL) {
                    tty_path = optarg;
                }
                break;
            case 'r':
                i = sscanf(optarg, "%u", &arg_u);
                if (i != 1) {
                    printf("ERROR: argument parsing of -r argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    radio_idx = (uint8_t)arg_u;
                }
                break;
            case 'a':
                i = sscanf(optarg, "%x", &arg_u);
                if (i != 1) {
                    printf("ERROR: argument parsing of -a argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    reg_addr = (uint16_t)arg_u;
                }
                break;
            case 'v':
                i = sscanf(optarg, "%x", &arg_u);
                if (i != 1) {
                    printf("ERROR: argument parsing of -v argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    reg_val_wr = (uint8_t)arg_u;
                }
                break;

            default:
                printf("ERROR: unkown argument. Use -h to print help\n");
                return EXIT_FAILURE;
        }
    }

    printf("### LoRa 2.4GHz Gateway - Radio Register Read/Write ###\n");

    /*  */
    fd = mcu_open(tty_path);
    if (fd == -1) {
        printf("ERROR: failed to connect\n");
        return EXIT_FAILURE;
    }

    x = mcu_ping(fd, &gw_info);
    if (x != 0) {
        printf("ERROR: failed to ping the concentrator\n");
        return EXIT_FAILURE;
    }

    conf.enable = true;
    conf.freq_hz = 2425000000;
    conf.datarate = DR_LORA_SF12;
    conf.bandwidth = BW_800KHZ;
    conf.rssi_offset = 0.0;
    conf.sync_word = LORA_SYNC_WORD_PUBLIC;
    x = mcu_config_rx(fd, radio_idx, &conf);
    if (x != 0) {
        printf("ERROR: failed to configure radio %u\n", 0);
        return EXIT_FAILURE;
    }

    /*  */
    x = mcu_read_register(fd, radio_idx, reg_addr, &reg_val);
    if (x != 0) {
        printf("ERROR: failed to connect\n");
        return EXIT_FAILURE;
    }
    printf("Read register 0x%04X:  0x%02X\n", reg_addr, reg_val);

    printf("Write register 0x%04X: 0x%02X\n", reg_addr, reg_val_wr);
    x = mcu_write_register(fd, radio_idx, reg_addr, reg_val_wr);
    if (x != 0) {
        printf("ERROR: failed to connect\n");
        return EXIT_FAILURE;
    }

    x = mcu_read_register(fd, radio_idx, reg_addr, &reg_val);
    if (x != 0) {
        printf("ERROR: failed to connect\n");
        return EXIT_FAILURE;
    }
    printf("Read register 0x%04X:  0x%02X\n", reg_addr, reg_val);

    /*  */
    x = mcu_close(fd);
    if (x != 0) {
        printf("ERROR: failed to disconnect\n");
        return EXIT_FAILURE;
    }


    printf("### Exiting ###\n");

    return 0;
}
