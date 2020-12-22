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
#define RX_DELAY_MS 10
#define NB_PKT_MAX 8

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
    printf(" -f <float> LoRa channel frequency in MHz, ]2400..2500[\n");
    printf(" -s <uint>  LoRa channel datarate 0:random, [5..12]\n");
    printf(" -b <uint>  LoRa channel bandwidth in khz 0:random, [200, 400, 800, 1600]\n");
    printf(" -t <uint>  Delay between 2 lgw_receive() requests, in milliseconds (default is %dms, min is %ums)\n", RX_DELAY_MS, RX_DELAY_MS);
    printf( "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n" );
    printf(" --loop     Number of loops for HAL start/stop (HAL unitary test)\n");
    printf(" --config   Send a packet to the end-node to configure it with TX_APP with given SF and BW\n");
    printf(" --priv       Use LoRa sync word for private network (0x12)\n");
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
    int i, j;
    double arg_d = 0.0;
    unsigned int arg_u;
    struct timeval now;
    int32_t tmst_diff;
    bool config_end_node = false;
    bool lorawan_public = true;

    uint32_t ft = 2425000000;
    e_spreading_factor sf = DR_LORA_SF12;
    e_bandwidth bw_khz = BW_800KHZ;
    unsigned int nb_loop = 0, cnt_loop;
    int nb_pkt = 0, nb_pkt_total = 0;
    unsigned long rx_delay = RX_DELAY_MS;

    struct lgw_conf_board_s boardconf;
    struct lgw_conf_channel_rx_s channelconf;
    struct lgw_pkt_rx_s pkt[NB_PKT_MAX];
    struct lgw_pkt_tx_s txpk;

    /* TTY interfaces */
    const char tty_path_default[] = TTY_PATH_DEFAULT;
    const char * tty_path = tty_path_default;

    static struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */

    /* Parameter parsing */
    int option_index = 0;
    static struct option long_options[] = {
        {"loop", required_argument, 0, 0},
        {"config", 0, 0, 0},
        {"priv", 0, 0, 0},
        {0, 0, 0, 0}
    };

    /* parse command line options */
    while ((i = getopt_long (argc, argv, "hf:s:b:d:t:", long_options, &option_index)) != -1) {
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
            case 'f': /* <float> Radio TX frequency in MHz */
                i = sscanf(optarg, "%lf", &arg_d);
                if (i != 1 || (arg_d < 2400) || (arg_d > 2500)) {
                    printf("ERROR: argument parsing of -f argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    ft = (uint32_t)((arg_d*1e6) + 0.5); /* .5 Hz offset to get rounding instead of truncating */
                }
                break;
            case 's': /* <uint> LoRa datarate */
                i = sscanf(optarg, "%u", &arg_u);
                if ((i != 1) || (arg_u < 5) || (arg_u > 12)) {
                    printf("ERROR: argument parsing of -s argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    sf = (uint8_t)arg_u;
                }
                break;
            case 'b': /* <uint> LoRa bandwidth in khz */
                i = sscanf(optarg, "%u", &arg_u);
                if (i != 1) {
                    printf("ERROR: argument parsing of -b argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    switch (arg_u) {
                        case 200:
                        case 203:
                            bw_khz = BW_200KHZ;
                            break;
                        case 400:
                        case 406:
                            bw_khz = BW_400KHZ;
                            break;
                        case 800:
                        case 812:
                            bw_khz = BW_800KHZ;
                            break;
                        case 1600:
                        case 1625:
                            bw_khz = BW_1600KHZ;
                            break;
                        default:
                            printf("ERROR: argument parsing of -b argument. Use -h to print help\n");
                            return EXIT_FAILURE;
                    }
                }
                break;
            case 't': /* <uint> RX delay */
                i = sscanf(optarg, "%u", &arg_u);
                if ((i != 1) || (arg_u < RX_DELAY_MS)) {
                    printf("ERROR: argument parsing of -t argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    rx_delay = (unsigned int)arg_u;
                }
                break;
            case 0:
                if (strcmp(long_options[option_index].name, "loop") == 0) {
                    i = sscanf(optarg, "%u", &arg_u);
                    if (i != 1) {
                        printf("ERROR: argument parsing of --loop argument. Use -h to print help\n");
                        return EXIT_FAILURE;
                    } else {
                        nb_loop = arg_u;
                    }
                } else if (strcmp(long_options[option_index].name, "config") == 0) {
                    config_end_node = true;
                } else if (strcmp(long_options[option_index].name, "priv") == 0) {
                    lorawan_public = false;
                } else {
                    printf("ERROR: argument parsing options. Use -h to print help\n");
                    return EXIT_FAILURE;
                }
                break;
            default:
                printf("ERROR: argument parsing\n");
                usage();
                return EXIT_FAILURE;
        }
    }

    printf("### LoRa 2.4GHz Gateway - HAL RX ###\n");
    printf("Waiting for LoRa packets on %u Hz (BW %u kHz, SF %i)\n", ft, lgw_get_bw_khz(bw_khz), sf);

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

    /* Configure RX channels */
    for (i = 0; i < LGW_RX_CHANNEL_NB_MAX; i++) {
        channelconf.enable = true;
        channelconf.freq_hz = ft;
        channelconf.datarate = sf;
        channelconf.bandwidth = bw_khz;
        channelconf.rssi_offset = 0.0;
        channelconf.sync_word = (lorawan_public == true) ? LORA_SYNC_WORD_PUBLIC : LORA_SYNC_WORD_PRIVATE;
        if (lgw_channel_rx_setconf(i, &channelconf) != 0) {
            printf("ERROR: failed to configure channel %u\n", i);
            return EXIT_FAILURE;
        }
    }


    /* Loop until user quits */
    cnt_loop = 0;
    while ((quit_sig != 1) && (exit_sig != 1)) {
        cnt_loop += 1;

        /* Connect, configure and start the LoRa concentrator */
        if (lgw_start() != 0) {
            return EXIT_FAILURE;
        }

        /* Configure mote (for automatic testing bench) */
        if (config_end_node == true) {
            txpk.freq_hz = 2403000000;
            txpk.tx_mode = IMMEDIATE;
            txpk.coderate = CR_LORA_LI_4_8;
            txpk.datarate = 5;
            txpk.bandwidth = BW_800KHZ;
            txpk.invert_pol = true;
            txpk.no_crc = true;
            txpk.no_header = false;
            txpk.preamble = 8;
            txpk.sync_word = (lorawan_public == true) ? LORA_SYNC_WORD_PUBLIC : LORA_SYNC_WORD_PRIVATE;
            txpk.rf_power = 0;
            txpk.size = 3;

            /* Config payload format:
                | SF | BW | APP |
                SF: Spreading Factor - 1 byte
                BW: Bandwidth - 1 byte
                APP: Application (TX, RX, LoRaWAN)
            */
            txpk.payload[0] = sf;
            switch (bw_khz) {
                case BW_200KHZ:
                    txpk.payload[1] = 0;
                    break;
                case BW_400KHZ:
                    txpk.payload[1] = 1;
                    break;
                case BW_800KHZ:
                    txpk.payload[1] = 2;
                    break;
                case BW_1600KHZ:
                    txpk.payload[1] = 3;
                    break;
                default:
                    printf("ERROR: bandwidth not supported\n");
                    return EXIT_FAILURE;
            }
            txpk.payload[2] = 0; /* TX app */
            if (lgw_send(&txpk) != 0) {
                printf("ERROR: lgw_send() failed for mote config\n");
                return EXIT_FAILURE;
            }
            wait_ms(1000); /* Wait for the mote to be in RX */
        }

        /* Start receiving packets */
        nb_pkt_total = 0;
        while (((nb_pkt_total < (int)nb_loop) || nb_loop == 0) && (quit_sig != 1) && (exit_sig != 1)) {
            nb_pkt = lgw_receive(NB_PKT_MAX, pkt);
            if (nb_pkt == -1) {
                printf("ERROR: lgw_receive failed\n");
                break;
            } else if (nb_pkt == 0) {
                /* avoid overflowing the com when no packet */
                wait_ms(rx_delay);
            } else {
                gettimeofday(&now, NULL);
                nb_pkt_total += nb_pkt;
                printf("%ld.%06ld: Received %d packets total:%d loop:%u\n", now.tv_sec, now.tv_usec, nb_pkt, nb_pkt_total, cnt_loop);
                for (i = 0; i < nb_pkt; i++) {
                    printf("pkt[%d]:{count:%u,size:%u,rssi:%.0f,snr:%.0f,foff:%i,data:", i, pkt[i].count_us, pkt[i].size, pkt[i].rssi, pkt[i].snr, pkt[i].foff_hz);
                    for (j = 0; j < pkt[i].size; j++) {
                        printf("%02X", pkt[i].payload[j]);
                    }
                    printf("}\n");
                }

                if (nb_pkt != LGW_RX_CHANNEL_NB_MAX) {
                    printf("\nWARNING: fetched only %d/%d packets at once\n\n", nb_pkt, LGW_RX_CHANNEL_NB_MAX);
                } else {
                    /* check timestamp diff for packets which arrived at the same time */
                    for (i = 1; i < nb_pkt; i++) {
                        tmst_diff = (int32_t)(pkt[i].count_us - pkt[0].count_us);
                        /* Tolerate a 100µs diff */
                        if ((tmst_diff > 100) || (tmst_diff < -100)) {
                            printf("ERROR: count_us between chan0 and chan%d differs too much (%dus)\n", i, tmst_diff);
                            lgw_stop();
                            return EXIT_FAILURE;
                        }
                    }
                }
            }
        }

        /* Stop the LoRa concentrator */
        if (lgw_stop() != 0) {
            return EXIT_FAILURE;
        }
    }

    printf("### Exiting ###\n");

    return 0;
}
