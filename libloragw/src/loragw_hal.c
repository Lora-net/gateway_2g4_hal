/*!
 * \brief     LoRa 2.4GHz concentrator Hardware Abstraction Layer
 *
 * License: Revised BSD 3-Clause License, see LICENSE.TXT file include in the project
 */

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf */
#include <string.h>     /* memcpy */
#include <math.h>       /* ceil */

#include "loragw_hal.h"
#include "loragw_mcu.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_HAL == 1
    #define DEBUG_MSG(str)                fprintf(stderr, str)
    #define DEBUG_PRINTF(fmt, args...)    fprintf(stderr, fmt, args)
    #define DEBUG_ARRAY(a,b,c)            for(a=0;a<b;++a) fprintf(stderr,"%x.",c[a]);fprintf(stderr,"end\n")
    #define CHECK_NULL(a)                 if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return -1;}
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
    #define DEBUG_ARRAY(a,b,c)            for(a=0;a!=0;){}
    #define CHECK_NULL(a)                 if(a==NULL){return -1;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS & TYPES -------------------------------------------- */

/* Version string, used to identify the library version/options once compiled */
const char lgw_version_string[] = "Version: " LIBLORAGW_VERSION ";";
const char mcu_version_string[] = "01.00.01";

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

/*
The following static variables are the configuration set that the user can
modify using board_setconf, channel_rx_setconffunctions.
The functions _start and _send then use that set to configure the hardware.

Parameters validity and coherency is verified by the _setconf functions and
the _start and _send functions assume they are valid.
*/

static char mcu_tty_path[64];
static int  mcu_fd;

static bool lgw_is_started;

static struct lgw_conf_channel_rx_s rx_channel[LGW_RX_CHANNEL_NB_MAX];
static struct lgw_conf_channel_tx_s tx_channel;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_board_setconf(const struct lgw_conf_board_s * conf) {
    CHECK_NULL(conf);

    /* check if the concentrator is running */
    if (lgw_is_started == true) {
        printf("ERROR: CONCENTRATOR IS RUNNING, STOP IT BEFORE TOUCHING CONFIGURATION\n");
        return -1;
    }

    /* set internal config according to parameters */
    strncpy(mcu_tty_path, conf->tty_path, sizeof mcu_tty_path);

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_channel_rx_setconf(uint8_t channel, const struct lgw_conf_channel_rx_s * conf) {
    CHECK_NULL(conf);
    if (channel >= LGW_RX_CHANNEL_NB_MAX) {
        printf("ERROR: invalid channel number\n");
    }

    /* check if the concentrator is running */
    if (lgw_is_started == true) {
        printf("ERROR: CONCENTRATOR IS RUNNING, STOP IT BEFORE TOUCHING CONFIGURATION\n");
        return -1;
    }

    /* set internal config according to parameters */
    rx_channel[channel].enable = conf->enable;
    rx_channel[channel].freq_hz = conf->freq_hz;
    rx_channel[channel].datarate = conf->datarate;
    rx_channel[channel].bandwidth = conf->bandwidth;
    rx_channel[channel].rssi_offset = conf->rssi_offset;
    rx_channel[channel].sync_word = conf->sync_word;

    if (conf->enable == true) {
        DEBUG_PRINTF("INFO: Setting channel %u configuration => en:%d freq:%u sf:%d bw:%ukhz rssi_offset:%.1f sync_word:0x%02X\n",   channel,
                                                                                                    rx_channel[channel].enable,
                                                                                                    rx_channel[channel].freq_hz,
                                                                                                    rx_channel[channel].datarate,
                                                                                                    lgw_get_bw_khz(rx_channel[channel].bandwidth),
                                                                                                    rx_channel[channel].rssi_offset,
                                                                                                    rx_channel[channel].sync_word);
    } else {
        DEBUG_PRINTF("INFO: Channel %u is disabled\n", channel);
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_channel_tx_setconf(const struct lgw_conf_channel_tx_s * conf) {
    CHECK_NULL(conf);

    /* check if the concentrator is running */
    if (lgw_is_started == true) {
        printf("ERROR: CONCENTRATOR IS RUNNING, STOP IT BEFORE TOUCHING CONFIGURATION\n");
        return -1;
    }

    /* set internal config according to parameters */
    tx_channel.enable = conf->enable;

    DEBUG_PRINTF("INFO: Setting TX %s\n", (tx_channel.enable == true) ? "Enabled" : "Disabled");

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_start(void) {
    int i;
    uint8_t idx;
    s_ping_info gw_info;
    s_status status;

    /* check if the concentrator is running */
    if (lgw_is_started == true) {
        printf("ERROR: CONCENTRATOR IS ALREADY RUNNING\n");
        return -1;
    }

    DEBUG_PRINTF("## opening %s\n", mcu_tty_path);
    mcu_fd = mcu_open(mcu_tty_path);
    if (mcu_fd == -1) {
        return -1;
    }

    /* Get information from the connected concentrator (mandatory) */
    if (mcu_ping(mcu_fd, &gw_info) != 0) {
        return -1;
    }

    /* Check MCU version (ignore first char of the received version (release/debug) */
    if (strncmp(gw_info.version + 1, mcu_version_string, sizeof mcu_version_string) != 0) {
        printf("ERROR: MCU version mismatch (expected:%s, got:%s)\n", mcu_version_string, gw_info.version);
        return -1;
    }
    printf("INFO: Concentrator MCU version is %s\n", gw_info.version);

    /* Reset RX radios */
    if (mcu_reset(mcu_fd, RESET_TYPE__RX_ALL) != 0) {
        printf("ERROR: Failed to reset concentrator RX radios\n");
        return -1;
    }

    /* Reset TX radio */
    if (mcu_reset(mcu_fd, RESET_TYPE__TX) != 0) {
        printf("ERROR: Failed to reset concentrator TX radios\n");
        return -1;
    }

    /* Get status */
    if (mcu_get_status(mcu_fd, &status) != 0) {
        printf("ERROR: Failed to get concentrator status\n");
        return -1;
    }

    /* Configure RX channels */
    for (i = 0; i < gw_info.nb_radio_rx; i++) {
        /* Set index to configure radio #1 first (TODO: to be removed) */
        idx = (i + 1) % 3;
        /* Configure radio */
        if (rx_channel[idx].enable == true) {
            /* TODO: enforce radio #1 to be enabled. Temporary workaround until hardware is fixed */
            if (rx_channel[1].enable == false) {
                printf("ERROR: Channel 1 cannot be disabled (radio #1 needs to be configured)\n");
                return -1;
            }

            printf("INFO: Configuring RX channel %d => freq:%u sf:%d bw:%ukhz\n",   idx,
                                                                                    rx_channel[idx].freq_hz,
                                                                                    rx_channel[idx].datarate,
                                                                                    lgw_get_bw_khz(rx_channel[idx].bandwidth));
            if (mcu_config_rx(mcu_fd, idx, &rx_channel[idx]) != 0) {
                printf("ERROR: Failed to configure radio #%u\n", idx);
                return -1;
            }
        }
    }

    lgw_is_started = true;

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_stop(void) {
    lgw_is_started = false;

    /* Reset concentrator RX radios */
    if (mcu_reset(mcu_fd, RESET_TYPE__RX_ALL) != 0) {
        printf("WARNING: FAILED TO RESET CONCENTRATOR RX RADIOS\n");
    }

    /* Reset concentrator TX radio */
    if (mcu_reset(mcu_fd, RESET_TYPE__TX) != 0) {
        printf("WARNING: FAILED TO RESET CONCENTRATOR TX RADIO\n");
    }

    DEBUG_PRINTF("## closing %s\n", mcu_tty_path);
    mcu_close(mcu_fd);

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_receive(uint8_t max_pkt, struct lgw_pkt_rx_s * pkt_data) {
    uint8_t nb_pkt_fetch; /* loop variable and return value */
    s_status status;
    int i;

    CHECK_NULL(pkt_data);

    /* check if the concentrator is running */
    if (lgw_is_started == false) {
        printf("ERROR: CONCENTRATOR IS NOT RUNNING, START IT BEFORE RECEIVING\n");
        return -1;
    }

    /* Get packets from the concentrator */
    if (mcu_receive(mcu_fd, max_pkt, pkt_data, &nb_pkt_fetch) != 0) {
        return -1;
    }

    /* Get RX status (for info) */
    if (mcu_get_status(mcu_fd, &status) != 0) {
        return -1;
    }
    for (i = 0; i < (int)mcu_get_nb_rx_radio(); i++) {
        if (status.rx_crc_ok[i] > 0) {
            DEBUG_PRINTF("INFO: [%d] Number of packets received with CRC OK:  %u\n", i, status.rx_crc_ok[i]);
        }
        if (status.rx_crc_err[i] > 0) {
            DEBUG_PRINTF("INFO: [%d] Number of packets received with CRC ERR: %u\n", i, status.rx_crc_err[i]);
        }
    }

    /* Update missing metadata */
    for (i = 0; i < nb_pkt_fetch; i++) {
        /* channel is already set */
        /* count_us is already set */
        /* snr is already set */
        pkt_data[i].freq_hz = rx_channel[pkt_data[i].channel].freq_hz;
        pkt_data[i].status = STAT_CRC_OK;
        pkt_data[i].modulation = MOD_LORA;
        pkt_data[i].bandwidth = rx_channel[pkt_data[i].channel].bandwidth;
        pkt_data[i].datarate = rx_channel[pkt_data[i].channel].datarate;
        pkt_data[i].coderate = CR_LORA_LI_4_7;

        /* Apply RSSI offset calibrated for the board/channel*/
        pkt_data[i].rssi += rx_channel[pkt_data[i].channel].rssi_offset;
    }

    return (int)nb_pkt_fetch;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_send(const struct lgw_pkt_tx_s * pkt_data) {
    CHECK_NULL(pkt_data);

    /* check if the concentrator is running */
    if (lgw_is_started == false) {
        printf("ERROR: CONCENTRATOR IS NOT RUNNING, START IT BEFORE RECEIVING\n");
        return -1;
    }

    /* Prepare non-blocking TX */
    if (mcu_prepare_tx(mcu_fd, pkt_data, false) != 0) {
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_status(e_status_type select, e_status * code) {
    e_tx_msg_status tx_status;

    CHECK_NULL(code);

    /* Get Status */
    if (select == TX_STATUS) {
        if (lgw_is_started == false) {
            *code = TX_OFF;
        } else {
            if (mcu_get_tx_status(mcu_fd, &tx_status) != 0) {
                printf("ERROR: Failed to get TX status\n");
                return -1;
            }
            switch(tx_status) {
                case TX_STATUS__IDLE:
                case TX_STATUS__DONE:
                    *code = TX_FREE;
                    break;
                case TX_STATUS__LOADED:
                    *code = TX_SCHEDULED;
                    break;
                case TX_STATUS__ON_AIR:
                    *code = TX_EMITTING;
                    break;
                default:
                    *code = TX_STATUS_UNKNOWN;
                    break;
            }
        }

    } else if (select == RX_STATUS) {
        if (lgw_is_started == false) {
            *code = RX_OFF;
        } else {
            *code = RX_ON;
        }
    } else {
        DEBUG_MSG("ERROR: SELECTION INVALID, NO STATUS TO RETURN\n");
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_abort_tx(void) {
    /* Reset concentrator TX radio */
    if (mcu_reset(mcu_fd, RESET_TYPE__TX) != 0) {
        printf("ERROR: Failed to reset concentrator TX radio\n");
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_get_trigcnt(uint32_t * trig_cnt_us) {
    s_status status;

    CHECK_NULL(trig_cnt_us);

    /* check if the concentrator is running */
    if (lgw_is_started == false) {
        printf("ERROR: CONCENTRATOR IS NOT RUNNING\n");
        return -1;
    }

    /* Get counter from status */
    if (mcu_get_status(mcu_fd, &status) == -1) {
        return -1;
    }

    *trig_cnt_us = status.pps_time_us;

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_get_instcnt(uint32_t * inst_cnt_us) {
    s_status status;

    CHECK_NULL(inst_cnt_us);

    /* check if the concentrator is running */
    if (lgw_is_started == false) {
        printf("ERROR: CONCENTRATOR IS NOT RUNNING\n");
        return -1;
    }

    /* Get counter from status */
    if (mcu_get_status(mcu_fd, &status) == -1) {
        return -1;
    }

    *inst_cnt_us = status.precise_time_us;

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const char* lgw_version_info(void) {
    return lgw_version_string;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_get_eui(uint64_t * eui) {
    s_ping_info gw_info;
    uint32_t ID1, ID2, ID3;
    uint8_t id[8];

    CHECK_NULL(eui);

    if (mcu_ping(mcu_fd, &gw_info) != 0) {
        return -1;
    }

    ID1 = gw_info.unique_id_high;
    ID2 = gw_info.unique_id_mid;
    ID3 = gw_info.unique_id_low;

    /* Build a 64-bits "EUI" from the 96-bits MCU device identifier number */
    /* TODO: the EUI is not guaranteed to be unique */
    id[7] = ( ID1 + ID3 ) >> 24;
    id[6] = ( ID1 + ID3 ) >> 16;
    id[5] = ( ID1 + ID3 ) >> 8;
    id[4] = ( ID1 + ID3 ) >> 0;
    id[3] = ( ID2 ) >> 24;
    id[2] = ( ID2 ) >> 16;
    id[1] = ( ID2 ) >> 8;
    id[0] = ( ID2 ) >> 0;

    *eui  = (uint64_t)(id[7]) << 56;
    *eui |= (uint64_t)(id[6]) << 48;
    *eui |= (uint64_t)(id[5]) << 40;
    *eui |= (uint64_t)(id[4]) << 32;
    *eui |= (uint64_t)(id[3]) << 24;
    *eui |= (uint64_t)(id[2]) << 16;
    *eui |= (uint64_t)(id[1]) << 8;
    *eui |= (uint64_t)(id[0]) << 0;

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_get_temperature(float * temperature, e_temperature_src * source) {
    s_status status;

    CHECK_NULL(temperature);

    /* check if the concentrator is running */
    if (lgw_is_started == false) {
        printf("ERROR: CONCENTRATOR IS NOT RUNNING\n");
        return -1;
    }

    /* Get temperature from status */
    if (mcu_get_status(mcu_fd, &status) == -1) {
        return -1;
    }

    *temperature = status.temperature.value;
    if (source != NULL) {
        *source = status.temperature.source;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint32_t lgw_time_on_air(const struct lgw_pkt_tx_s * pkt, double * result) {
    uint16_t bw = 0.0;
    double LocalTimeOnAir;
    double symbolPeriod;
    double fec_rate;
    double Nsymbol_header;
    double total_bytes_nb;
    double symbols_nb_preamble;
    double symbols_nb_data;
    double tx_bits_symbol;
    double tx_infobits_header;
    double tx_infobits_payload;

    CHECK_NULL(pkt);

    bool fine_synch = (pkt->datarate <= 6);
    bool long_interleaving = (pkt->coderate > 4);

    switch (pkt->bandwidth) {
        case BW_200KHZ:
            bw = 203;
            break;
        case BW_400KHZ:
            bw = 406;
            break;
        case BW_800KHZ:
            bw = 812;
            break;
        case BW_1600KHZ:
            bw = 1625;
            break;
        default:
            printf("ERROR: invalid bandwidth, failed to compute time on air\n");
            return 0;
    }

    symbolPeriod = pow(2, pkt->datarate) / (double)bw;

    if (long_interleaving) {
        int cr_is_7 = (pkt->coderate == 7) ? 1 : 0;
        fec_rate = 4.0 / (pkt->coderate + cr_is_7);
    } else {
        fec_rate = 4.0 / (4.0 + pkt->coderate);
    }

    total_bytes_nb = pkt->size + 2 * (pkt->no_crc == false ? 1 : 0);
    tx_bits_symbol = pkt->datarate - 2 * (pkt->datarate >= 11 ? 1 : 0);

    Nsymbol_header = (pkt->no_header == false) ? 20 : 0;
    tx_infobits_header = (pkt->datarate * 4 + (fine_synch ? 1 : 0) * 8 - 8 - Nsymbol_header);

    if (!long_interleaving) {
        tx_infobits_payload = MAX(0, 8 * total_bytes_nb - tx_infobits_header);
        symbols_nb_data = 8 + ceil(tx_infobits_payload / 4 / tx_bits_symbol) * (pkt->coderate + 4);
    } else {
        if (pkt->no_header == false) {
            if (tx_infobits_header < 8 * total_bytes_nb) {
                tx_infobits_header = MIN(tx_infobits_header, 8 * pkt->size);
            }
            tx_infobits_payload = MAX(0, 8 * total_bytes_nb - tx_infobits_header);
            symbols_nb_data = 8 + ceil(tx_infobits_payload / fec_rate / tx_bits_symbol);
        } else {
            double tx_bits_symbol_start = pkt->datarate - 2 + 2 * (fine_synch ? 1 : 0);
            double symbols_nb_start = ceil(8 * total_bytes_nb / fec_rate / tx_bits_symbol_start);
            if (symbols_nb_start < 8) {
                symbols_nb_data = symbols_nb_start;
            } else  {
                double tx_codedbits_header = tx_bits_symbol_start * 8;
                double tx_codedbits_payload = 8 * total_bytes_nb / fec_rate - tx_codedbits_header;
                symbols_nb_data = 8 + ceil(tx_codedbits_payload / tx_bits_symbol);
            }
        }
    }

    symbols_nb_preamble = pkt->preamble + 4.25 + 2 * (fine_synch ? 1 : 0);
    LocalTimeOnAir = (symbols_nb_preamble + symbols_nb_data) * symbolPeriod;

    /* Return result with full precision, and ceiled */
    if (result != NULL) {
        *result = LocalTimeOnAir;
    }
    return (uint32_t)(ceil(LocalTimeOnAir));
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint16_t lgw_get_bw_khz(e_bandwidth bandwidth) {
    switch (bandwidth) {
        case BW_200KHZ:
            return 200;
        case BW_400KHZ:
            return 400;
        case BW_800KHZ:
            return 800;
        case BW_1600KHZ:
            return 1600;
        default:
            printf("ERROR: bandwidth is invalid (%d)\n", bandwidth);
            return 0;
    }
}

/* --- EOF ------------------------------------------------------------------ */