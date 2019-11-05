/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    Minimum test program for the loragw_hal 'library'

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

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf */
#include <stdlib.h>     /* EXIT_FAILURE */
#include <signal.h>     /* sigaction */
#include <getopt.h>     /* getopt_long */
#include <string.h>     /* strcmp */
#include <sys/time.h>   /* gettimeofday */

#include "loragw_hal.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define TTY_PATH_DEFAULT "/dev/ttyACM0"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

/* Signal handling variables */
static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */

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
    printf(" -f <path>  File name to store counter values (print to console if not set)\n");
}

/* handle signals */
static void sig_handler(int sigio)
{
    if (sigio == SIGQUIT) {
        quit_sig = 1;
    }
    else if((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = 1;
    }
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv) {
    int i;
    uint32_t count_us;
    struct timeval now;
    FILE * fd = NULL;

    struct lgw_conf_board_s boardconf;
    struct lgw_conf_channel_rx_s channelconf;

    /* TTY interfaces */
    const char tty_path_default[] = TTY_PATH_DEFAULT;
    const char * tty_path = tty_path_default;

    static struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */

    /* Parameter parsing */
    int option_index = 0;
    static struct option long_options[] = {
        {0, 0, 0, 0}
    };

    /* parse command line options */
    while ((i = getopt_long (argc, argv, "hd:f:", long_options, &option_index)) != -1) {
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
            case 'f':
                if (optarg != NULL) {
                    /* Open file for logging */
                    fd = fopen(optarg, "w+");
                    if (fd == NULL) {
                        perror("ERROR: failed to open file - ");
                        return EXIT_FAILURE;
                    }
                }
                break;
            default:
                printf("ERROR: unknown argument\n");
                return EXIT_FAILURE;
        }
    }

    printf("### LoRa 2.4GHz Gateway - HAL COUNTER ###\n");

    /* Configure signal handling */
    sigemptyset( &sigact.sa_mask );
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction( SIGQUIT, &sigact, NULL );
    sigaction( SIGINT, &sigact, NULL );
    sigaction( SIGTERM, &sigact, NULL );

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

    while ((quit_sig != 1) && (exit_sig != 1)) {
        /* Get host time */
        gettimeofday(&now, NULL);

        /* Get gateway internal counter */
        if (lgw_get_instcnt(&count_us) != 0) {
            return EXIT_FAILURE;
        }

        if (fd != NULL) {
            fprintf(fd, "%ld.%06ld,%u\n", now.tv_sec, now.tv_usec, count_us);
        } else {
            printf("%ld.%06ld,%u\n", now.tv_sec, now.tv_usec, count_us);
        }
        wait_ms(100);
    }

    /* Stop the LoRa concentrator */
    if (lgw_stop() != 0) {
        return EXIT_FAILURE;
    }

    if (fd != NULL) {
        fclose(fd);
    }

    printf("### Exiting ###\n");

    return 0;
}
