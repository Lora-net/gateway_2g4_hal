/*!
 * \brief     Utility to get the concentrator EUI
 *
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>   /* PRIx64, PRIu64... */
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>     /* sigaction */
#include <getopt.h>     /* getopt_long */

#include "loragw_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define RAND_RANGE(min, max) (rand() % (max + 1 - min) + min)

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define TTY_PATH_DEFAULT "/dev/ttyACM0"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS ---------------------------------------------------- */

/* describe command line options */
void usage(void) {
    printf("Library version information: %s\n", lgw_version_info());
    printf("Available options:\n");
    printf(" -h print this help\n");
    printf(" -d [path]  TTY path to be used to access the concentrator\n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv)
{
    int i, x;

    struct lgw_conf_board_s boardconf;
    struct lgw_conf_channel_rx_s channelconf;
    uint64_t eui;

    /* SPI interfaces */
    const char tty_path_default[] = TTY_PATH_DEFAULT;
    const char * tty_path = tty_path_default;

    /* Parameter parsing */
    int option_index = 0;
    static struct option long_options[] = {
        {0, 0, 0, 0}
    };

    /* parse command line options */
    while ((i = getopt_long (argc, argv, "hd:", long_options, &option_index)) != -1) {
        switch (i) {
            case 'h':
                usage();
                return -1;
                break;

            case 'd':
                tty_path = optarg;
                break;

            default:
                printf("ERROR: argument parsing\n");
                usage();
                return -1;
        }
    }

    /* Configure the gateway */
    memset(&boardconf, 0, sizeof boardconf);
    strncpy(boardconf.tty_path, tty_path, sizeof boardconf.tty_path);
    if (lgw_board_setconf(&boardconf) != 0) {
        printf("ERROR: failed to configure board\n");
        return EXIT_FAILURE;
    }

    /* Disable all RX channels */
    for (i = 0; i < LGW_RX_CHANNEL_NB_MAX; i++) {
        channelconf.enable = false;
        if (lgw_channel_rx_setconf(i, &channelconf) != 0) {
            printf("ERROR: failed to configure channel %u\n", i);
            return EXIT_FAILURE;
        }
    }

    /* Connect, configure and start the LoRa concentrator */
    if (lgw_start() != 0) {
        return EXIT_FAILURE;
    }

    /* get the concentrator EUI */
    x = lgw_get_eui(&eui);
    if (x != LGW_HAL_SUCCESS) {
        printf("ERROR: failed to get concentrator EUI\n");
    } else {
        printf("\nINFO: concentrator EUI: 0x%016" PRIx64 "\n\n", eui);
    }

    /* Stop the gateway */
    x = lgw_stop();
    if (x != 0) {
        printf("ERROR: failed to stop the gateway\n");
        return EXIT_FAILURE;
    }

    return 0;
}

/* --- EOF ------------------------------------------------------------------ */
