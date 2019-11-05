/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    Utility to switch the concentrator in DFU boot mode

License: Revised BSD License, see LICENSE.TXT file include in the project
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

#include "loragw_mcu.h"

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
    int fd;

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

    /*  */
    fd = mcu_open(tty_path);
    if (fd == -1) {
        printf("ERROR: failed to connect\n");
        return EXIT_FAILURE;
    }

    /*  */
    x = mcu_boot(fd);
    if (x != 0) {
        printf("ERROR: failed to connect\n");
        return EXIT_FAILURE;
    }

    /*  */
    x = mcu_close(fd);
    if (x != 0) {
        printf("ERROR: failed to disconnect\n");
        return EXIT_FAILURE;
    }

    return 0;
}

/* --- EOF ------------------------------------------------------------------ */
