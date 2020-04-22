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
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv) {
    int i, x;
    int fd;
    s_ping_info gw_info;

    /* TTY interfaces */
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
                if (optarg != NULL) {
                    tty_path = optarg;
                }
                break;
            default:
                printf("ERROR: unkown argument. Use -h to print help\n");
                return EXIT_FAILURE;
        }
    }

    printf("### LoRa 2.4GHz Gateway - Reset MCU ###\n");

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

    x = mcu_reset(fd, RESET_TYPE__RX_ALL);
    if (x != 0) {
        printf("ERROR: failed to reset the concentrator RX radios\n");
        return EXIT_FAILURE;
    }

    x = mcu_reset(fd, RESET_TYPE__TX);
    if (x != 0) {
        printf("ERROR: failed to reset the concentrator TX radio\n");
        return EXIT_FAILURE;
    }

    x = mcu_reset(fd, RESET_TYPE__GTW);
    if (x != 0) {
        printf("ERROR: failed to reset the concentrator MCU\n");
        return EXIT_FAILURE;
    }

    x = mcu_close(fd);
    if (x != 0) {
        printf("ERROR: failed to disconnect\n");
        return EXIT_FAILURE;
    }

    printf("### Exiting ###\n");

    return 0;
}
