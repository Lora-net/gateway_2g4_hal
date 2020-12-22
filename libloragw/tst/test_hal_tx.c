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

#include "loragw_hal.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define TTY_PATH_DEFAULT "/dev/ttyACM0"

#define DEFAULT_FREQ_HZ  2425000000U

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
    printf(" -f <float> Radio TX frequency in MHz, ]2400..2500[\n");
    printf(" -s <uint>  LoRa datarate 0:random, [5..12]\n");
    printf(" -b <uint>  LoRa bandwidth in khz 0:random, [200, 400, 800, 1600]\n");
    printf(" -l <uint>  LoRa preamble length, [6..61440]\n");
    printf(" -n <uint>  Number of packets to be sent\n");
    printf(" -z <uint>  size of packets to be sent 0:random, [9..255], 0 for all sizes [szmin..szmax]\n");
    printf(" -p <int>   RF power in dBm [0..13]\n");
    printf(" -i         Send LoRa packet using inverted modulation polarity\n");
    printf(" -t         Delay between each packet sent in milliseconds [> 50ms]\n");
    printf( "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n" );
    printf(" --loop <int> Number of loops for HAL start/stop (HAL unitary test)\n");
    printf(" --trig       Use TIMESTAMP mode instead of IMMEDIATE\n");
    printf(" --per        Use PER measurement payload (32-bits counter on last 4 bytes)\n");
    printf(" --config     Send a packet to the end-node to configure it with TX_APP with given SF and BW\n");
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
    int i;
    double arg_d = 0.0;
    unsigned int arg_u;
    int arg_i;
    bool config_end_node = false;
    bool lorawan_public = true;

    uint32_t ft = DEFAULT_FREQ_HZ;
    int8_t rf_power = 13;
    e_spreading_factor sf = DR_LORA_SF12;
    e_coding_rate cr = CR_LORA_LI_4_8;
    e_bandwidth bw_khz = BW_800KHZ;
    uint32_t nb_pkt = 1;
    uint8_t size = 0, size_min = 9, size_max = 253;
    uint16_t preamble = 8;
    bool invert_pol = false;
    unsigned int nb_loop = 1, cnt_loop;
    uint32_t delay_ms = 0;
    uint32_t cnt_now;
    bool tx_mode_timestamp = false;
    e_status status;
    bool per_mode = false;

    struct lgw_conf_board_s boardconf;
    struct lgw_conf_channel_rx_s channelconf;
    struct lgw_pkt_tx_s pkt;
    struct lgw_pkt_tx_s txpk;

    /* TTY interfaces */
    const char tty_path_default[] = TTY_PATH_DEFAULT;
    const char * tty_path = tty_path_default;

    static struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */

    /* Parameter parsing */
    int option_index = 0;
    static struct option long_options[] = {
        {"loop", required_argument, 0, 0},
        {"trig", 0, 0, 0},
        {"per", 0, 0, 0},
        {"szmin", required_argument, 0, 0},
        {"szmax", required_argument, 0, 0},
        {"config", 0, 0, 0},
        {"priv", 0, 0, 0},
        {0, 0, 0, 0}
    };

    /* parse command line options */
    while ((i = getopt_long (argc, argv, "hif:s:b:n:z:p:l:d:t:", long_options, &option_index)) != -1) {
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
            case 'i': /* Send packet using inverted modulation polarity */
                invert_pol = true;
                break;
            case 'l': /* <uint> LoRa preamble length */
                i = sscanf(optarg, "%u", &arg_u);
                if ((i != 1) || (arg_u > 61440)) {
                    printf("ERROR: argument parsing of -l argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    preamble = (uint16_t)arg_u;
                }
                break;
            case 't': /* <uint> delay_ms */
                i = sscanf(optarg, "%u", &arg_u);
                if ((i != 1) || (arg_u < 50)) {
                    printf("ERROR: argument parsing of -t argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    delay_ms = (uint32_t)arg_u;
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
            case 'n': /* <uint> Number of packets to be sent */
                i = sscanf(optarg, "%u", &arg_u);
                if (i != 1) {
                    printf("ERROR: argument parsing of -n argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    nb_pkt = (uint32_t)arg_u;
                }
                break;
            case 'p': /* <int> RF power */
                i = sscanf(optarg, "%d", &arg_i);
                if (i != 1) {
                    printf("ERROR: argument parsing of -p argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    rf_power = (int8_t)arg_i;
                }
                break;
            case 'z': /* <uint> packet size */
                i = sscanf(optarg, "%u", &arg_u);
                if ((i != 1) || ((arg_u != 0) && (arg_u < 9)) || (arg_u > 255)) {
                    printf("ERROR: argument parsing of -z argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    size = (uint8_t)arg_u;
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
                } else if (strcmp(long_options[option_index].name, "trig") == 0) {
                    tx_mode_timestamp = true;
                } else if (strcmp(long_options[option_index].name, "per") == 0) {
                    per_mode = true;
                } else if (strcmp(long_options[option_index].name, "szmin") == 0) {
                    i = sscanf(optarg, "%u", &arg_u);
                    if ((i != 1) || (arg_u < 9) || (arg_u > 255)) {
                        printf("ERROR: argument parsing of --szmin argument. Use -h to print help\n");
                        return EXIT_FAILURE;
                    } else {
                        size_min = arg_u;
                    }
                } else if (strcmp(long_options[option_index].name, "szmax") == 0) {
                    i = sscanf(optarg, "%u", &arg_u);
                    if ((i != 1) || (arg_u < 9) || (arg_u > 255)) {
                        printf("ERROR: argument parsing of --szmax argument. Use -h to print help\n");
                        return EXIT_FAILURE;
                    } else {
                        size_max = arg_u;
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

    printf("### LoRa 2.4GHz Gateway - HAL TX ###\n");
    printf("Sending %i LoRa packets on %u Hz (BW %u kHz, SF %i, CR %i, %i bytes payload, %i symbols preamble, %s polarity) at %i dBm\n", nb_pkt, ft, lgw_get_bw_khz(bw_khz), sf, cr, size, preamble, (invert_pol == false) ? "non-inverted" : "inverted", rf_power);

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

    for (cnt_loop = 0; (cnt_loop < nb_loop) && (quit_sig != 1) && (exit_sig != 1); cnt_loop++) {
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
            txpk.payload[2] = 1; /* RX app */
            if (lgw_send(&txpk) != 0) {
                printf("ERROR: lgw_send() failed for mote config\n");
                return EXIT_FAILURE;
            }
            wait_ms(1000);
        }

        /* Configure TX */
        if (per_mode == false) {
            /* LoRaWAN payload */
            pkt.payload[0] = 0x40; /* Confirmed Data Up */
            pkt.payload[1] = 0xAB;
            pkt.payload[2] = 0xAB;
            pkt.payload[3] = 0xAB;
            pkt.payload[4] = 0xAB;
            pkt.payload[5] = 0x00; /* FCTrl */
            pkt.payload[6] = 0; /* FCnt */
            pkt.payload[7] = 0; /* FCnt */
            pkt.payload[8] = 0x02; /* FPort */
            for (i = 9; i < 255; i++) {
                pkt.payload[i] = i;
            }
        }

        for (i = 0; (i < (int)nb_pkt) && (quit_sig != 1) && (exit_sig != 1); i++) {
            /* Prepare params */
            if (tx_mode_timestamp == true) {
                lgw_get_instcnt(&cnt_now);
                pkt.tx_mode = TIMESTAMPED;
                pkt.count_us = cnt_now + 20E3; /* cannot program TX more than 200ms in advance */
            } else {
                pkt.tx_mode = IMMEDIATE;
                pkt.count_us = 0;
            }
            pkt.rf_power = rf_power;
            pkt.freq_hz = ft;
            pkt.bandwidth = bw_khz;
            pkt.datarate = sf;
            pkt.coderate = CR_LORA_LI_4_8;
            pkt.invert_pol = invert_pol;
            pkt.no_crc = true;
            pkt.no_header = false;
            pkt.preamble = preamble;
            pkt.sync_word = (lorawan_public == true) ? LORA_SYNC_WORD_PUBLIC : LORA_SYNC_WORD_PRIVATE;

            /* Set given size or set current size of "all sizes" mode */
            if (size != 0) {
                pkt.size = size;
            } else {
                pkt.size = i % (size_max + 1 - size_min) + size_min;
            }

            if (per_mode == false) {
                pkt.payload[6] = (uint8_t)(i >> 0); /* FCnt */
                pkt.payload[7] = (uint8_t)(i >> 8); /* FCnt */
            } else {
                /* Add counter on last 4 bytes for PER measurement */
                pkt.payload[pkt.size - 1] = (uint8_t)(i >> 0);
                pkt.payload[pkt.size - 2] = (uint8_t)(i >> 8);
                pkt.payload[pkt.size - 3] = (uint8_t)(i >> 16);
                pkt.payload[pkt.size - 4] = (uint8_t)(i >> 24);
            }

            /* Send packets */
            printf("-> sending %s packet %d (size:%u)\n", (tx_mode_timestamp == true) ? "scheduled" : "immediate", i, pkt.size);
            if (lgw_send(&pkt) != 0) {
                printf("ERROR: failed to send packet\n");
            }

            /* Wait for packet to be sent */
            do {
                if (lgw_status(TX_STATUS, &status) != 0) {
                    printf("ERROR: failed to get TX status\n");
                }
                wait_ms(10);
            } while ((status != TX_FREE) && (quit_sig != 1) && (exit_sig != 1));

            wait_ms(delay_ms);
        }

        /* Abort current TX if necessary */
        if (lgw_status(TX_STATUS, &status) == 0) {
            if (status != TX_FREE) {
                printf("INFO: aborting TX (status:%u)\n", status);
                if (lgw_abort_tx() != 0) {
                    printf("ERROR: failed to abort TX\n");
                }
            }
        }

        printf ("Nb packets sent:%u loop:%u\n\n", nb_pkt, cnt_loop + 1);

        /* Stop the LoRa concentrator */
        if (lgw_stop() != 0) {
            printf("ERROR: failed to stop the concentrator\n");
            return EXIT_FAILURE;
        }
    }

    printf("### Exiting ###\n");

    return 0;
}
