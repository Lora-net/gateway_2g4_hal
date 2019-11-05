/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    LoRa 2.4GHz concentrator MCU interface functions

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf */
#include <string.h>     /* memcpy */
#include <stdlib.h>     /* rand */
#include <fcntl.h>      /* open/close */
#include <errno.h>      /* perror */
#include <unistd.h>     /* read, write */
#include <termios.h>    /* POSIX terminal control definitions */

#include "loragw_mcu.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_MCU == 1
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

#define DEBUG_VERBOSE 0

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS & TYPES -------------------------------------------- */

/* Commands */
#define HEADER_CMD_SIZE  4
#define WRITE_SIZE_MAX 280
#define READ_SIZE_MAX 500

/*!
* \brief Represents the ramping time for radio power amplifier
*/
typedef enum {
    RADIO_RAMP_02_US = 0x00,
    RADIO_RAMP_04_US = 0x20,
    RADIO_RAMP_06_US = 0x40,
    RADIO_RAMP_08_US = 0x60,
    RADIO_RAMP_10_US = 0x80,
    RADIO_RAMP_12_US = 0xA0,
    RADIO_RAMP_16_US = 0xC0,
    RADIO_RAMP_20_US = 0xE0,
} RampTimes_t;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

/* Hardware */
static uint8_t nb_radio_rx = 0;
static uint8_t nb_radio_tx = 0;

static uint8_t buf_req[WRITE_SIZE_MAX];
static uint8_t buf_ack[READ_SIZE_MAX];

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

const char * cmd_get_str(const uint8_t cmd) {
    switch (cmd) {
        case ORDER_ID__REQ_PING:
            return "REQ_PING";
        case ORDER_ID__REQ_CONFIG_RX:
            return "REQ_CONFIG_RX";
        case ORDER_ID__REQ_PREPARE_TX:
            return "REQ_PREPARE_TX";
        case ORDER_ID__REQ_GET_STATUS:
            return "REQ_GET_STATUS";
        case ORDER_ID__REQ_BOOTLOADER_MODE:
            return "REQ_BOOTLOADER_MODE";
        case ORDER_ID__REQ_GET_RX_MSG:
            return "REQ_GET_RX_MSG";
        case ORDER_ID__REQ_GET_TX_STATUS:
            return "REQ_GET_TX_STATUS";
        case ORDER_ID__REQ_RESET:
            return "REQ_RESET";
        case ORDER_ID__REQ_SET_COEF_TEMP_RSSI:
            return "REQ_SET_COEF_TEMP_RSSI";
        case ORDER_ID__REQ_READ_REGS:
            return "ORDER_ID__REQ_READ_REGS";
        case ORDER_ID__REQ_WRITE_REGS:
            return "ORDER_ID__REQ_WRITE_REGS";
        default:
            return "UNKNOWN";
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint8_t cmd_get_id(const uint8_t * bytes) {
    return bytes[0];
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint16_t cmd_get_size(const uint8_t * bytes) {
    return (uint16_t)(bytes[1] << 8) | bytes[2];
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint8_t cmd_get_type(const uint8_t * bytes) {
    return bytes[3];
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int write_req(int fd, e_order_cmd cmd, uint16_t size, const uint8_t * payload) {
    uint8_t buf_w[HEADER_CMD_SIZE];
    int n;

    /* Write command header */
    buf_w[0] = rand() % 255;
    buf_w[1] = (uint8_t)(size >> 8); /* MSB */
    buf_w[2] = (uint8_t)(size >> 0); /* LSB */
    buf_w[3] = cmd;
    n = write(fd, buf_w, HEADER_CMD_SIZE);
    if (n < 0) {
        printf("ERROR: failed to write command header to com port\n");
        return -1;
    }

    /* Write command payload */
    if (size > 0) {
        if (payload == NULL) {
            printf("ERROR: invalid payload\n");
            return -1;
        }
        n = write(fd, payload, size);
        if (n < 0) {
            printf("ERROR: failed to write command payload to com port\n");
            return -1;
        }
    }

    DEBUG_PRINTF("\nINFO: write_req 0x%02X (%s) done, id:0x%02X\n", cmd, cmd_get_str(cmd), buf_w[0]);

#if DEBUG_VERBOSE
    int i;
    for (i = 0; i < 4; i++) {
        DEBUG_PRINTF("%02X ", buf_w[i]);
    }
    for (i = 0; i < size; i++) {
        DEBUG_PRINTF("%02X ", payload[i]);
    }
    DEBUG_MSG("\n");
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int read_ack(int fd, uint8_t * buf, size_t buf_size) {
    int i, n;
    size_t size;
    int nb_read = 0;

    /* Read message header first */
    n = read(fd, &buf[0], (size_t)HEADER_CMD_SIZE);
    if (errno == EINTR) {
        printf("INFO: syscall was interrupted, continue...\n");
        /* TODO: check what to do here */
        return -1;
    } else if (n == -1) {
        perror("ERROR: Unable to read /dev/ttyACMx - ");
        return -1;
    } else {
        DEBUG_PRINTF("INFO: read %d bytes for header from gateway\n", n);
        nb_read += n;
    }

    /* debug print */
    for (i = 0; i < (int)(HEADER_CMD_SIZE); i++) {
        DEBUG_PRINTF("%02X ", buf[i]);
    }
    DEBUG_MSG("\n");

    /* Get remaining payload size (metadata + pkt payload) */
    size  = (size_t)buf[CMD_OFFSET__SIZE_MSB] << 8;
    size |= (size_t)buf[CMD_OFFSET__SIZE_LSB] << 0;
    if (((size_t)HEADER_CMD_SIZE + size) > buf_size) {
        printf("ERROR: not enough memory to store all data (%zd)\n", (size_t)HEADER_CMD_SIZE + size);
        return -1;
    }

    /* Read payload if any */
    if (size > 0) {
        do {
            n = read(fd, &buf[nb_read], size - (nb_read - HEADER_CMD_SIZE));
            if (errno == EINTR) {
                printf("INFO: syscall was interrupted, continue...\n");
                /* TODO: check what to do here */
                return -1;
            } else if (n == -1) {
                perror("ERROR: Unable to read /dev/ttyACMx - ");
                return -1;
            } else {
                DEBUG_PRINTF("INFO: read %d bytes from gateway\n", n);
                nb_read += n;
            }
        } while ((nb_read - HEADER_CMD_SIZE) < (int)size); /* we want to read only the expected payload, not more */

        /* debug print */
        for (i = HEADER_CMD_SIZE; i < (int)(HEADER_CMD_SIZE + size); i++) {
            DEBUG_PRINTF("%02X ", buf[i]);
        }
        DEBUG_MSG("\n");
    }

    return nb_read;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_get_status(const uint8_t * payload, s_status * status) {
    int i;
    int16_t temperature;

    /* sanity checks */
    if ((payload == NULL) || (status == NULL)) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_GET_STATUS) {
        printf("ERROR: wrong ACK type for GET_STATUS (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_GET_STATUS, cmd_get_type(payload));
        return -1;
    }

    /* payload info */
    status->system_time_ms = bytes_be_to_uint32_le(&payload[HEADER_CMD_SIZE + ACK_GET_STATUS__SYSTEM_TIME_31_24]);

    status->precise_time_us = bytes_be_to_uint32_le(&payload[HEADER_CMD_SIZE + ACK_GET_STATUS__PRECISE_TIMER_31_24]);

    status->pps_status = (e_pps_status)payload[HEADER_CMD_SIZE + ACK_GET_STATUS__PPS_STATUS];

    status->pps_time_us = bytes_be_to_uint32_le(&payload[HEADER_CMD_SIZE + ACK_GET_STATUS__PPS_TIME_31_24]);

    temperature = (int16_t)(payload[HEADER_CMD_SIZE + ACK_GET_STATUS__TEMPERATURE_15_8] << 8) |
                  (int16_t)(payload[HEADER_CMD_SIZE + ACK_GET_STATUS__TEMPERATURE_7_0]  << 0);
    status->temperature = (float)temperature / 100.0;

    for (i = 0; i < nb_radio_rx; i++) {
        status->rx_crc_ok[i]   = (uint16_t)(payload[HEADER_CMD_SIZE + ACK_GET_STATUS__RX_STATUS + (4 * i) + 0] << 8);
        status->rx_crc_ok[i]  |= (uint16_t)(payload[HEADER_CMD_SIZE + ACK_GET_STATUS__RX_STATUS + (4 * i) + 1] << 0);

        status->rx_crc_err[i]  = (uint16_t)(payload[HEADER_CMD_SIZE + ACK_GET_STATUS__RX_STATUS + (4 * i) + 2] << 8);
        status->rx_crc_err[i] |= (uint16_t)(payload[HEADER_CMD_SIZE + ACK_GET_STATUS__RX_STATUS + (4 * i) + 3] << 0);
    }

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_GET_STATUS\n");
    DEBUG_PRINTF("   id:            0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:          %u\n", cmd_get_size(payload));
    DEBUG_PRINTF("   sys_time:      %u\n", status->system_time_ms);
    DEBUG_PRINTF("   precise_time:  %u\n", status->precise_time_us);
    DEBUG_PRINTF("   pps_status:    0x%02X\n", status->pps_status);
    DEBUG_PRINTF("   pps_time:      %u\n", status->pps_time_us);
    DEBUG_PRINTF("   temperature:   %.1f\n", status->temperature);
    for (i = 0; i < nb_radio_rx; i++) {
        DEBUG_PRINTF("   rx_crc_ok[%d]:  %u\n", i, status->rx_crc_ok[i]);
        DEBUG_PRINTF("   rx_crc_err[%d]: %u\n", i, status->rx_crc_err[i]);
    }
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_ping(const uint8_t * payload, s_ping_info * info) {
    /* sanity checks */
    if ((payload == NULL) || (info == NULL)) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_PING) {
        printf("ERROR: wrong ACK type for PING (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_PING, cmd_get_type(payload));
        return -1;
    }

    /* payload info */
    info->unique_id_high = bytes_be_to_uint32_le(&payload[HEADER_CMD_SIZE + ACK_PING__UNIQUE_ID_0]);
    info->unique_id_mid  = bytes_be_to_uint32_le(&payload[HEADER_CMD_SIZE + ACK_PING__UNIQUE_ID_4]);
    info->unique_id_low  = bytes_be_to_uint32_le(&payload[HEADER_CMD_SIZE + ACK_PING__UNIQUE_ID_8]);

    memcpy(info->version, &payload[HEADER_CMD_SIZE + ACK_PING__VERSION_0], (sizeof info->version) - 1);
    info->version[(sizeof info->version) - 1] = '\0'; /* terminate string */

    info->nb_radio_tx = payload[HEADER_CMD_SIZE + ACK_PING__NB_RADIO_TX];

    info->nb_radio_rx = payload[HEADER_CMD_SIZE + ACK_PING__NB_RADIO_RX];

    /* store local context */
    nb_radio_rx = info->nb_radio_rx;
    nb_radio_tx = info->nb_radio_tx;

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_PING\n");
    DEBUG_PRINTF("   id:           0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:         %u\n", cmd_get_size(payload));
    DEBUG_PRINTF("   unique_id:    0x%08X%08X%08X\n", info->unique_id_high, info->unique_id_mid, info->unique_id_low);
    DEBUG_PRINTF("   FW version:   %s\n", info->version);
    DEBUG_PRINTF("   nb_radio_tx:  %u\n", info->nb_radio_tx);
    DEBUG_PRINTF("   nb_radio_rx:  %u\n", info->nb_radio_rx);
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_prepare_tx(const uint8_t * payload, e_prepare_tx_status * tx_prepare_status) {
    /* sanity checks */
    if ((payload == NULL) || (tx_prepare_status == NULL)) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_PREPARE_TX) {
        printf("ERROR: wrong ACK type for PREPARE_TX (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_PREPARE_TX, cmd_get_type(payload));
        return -1;
    }

    /* payload info */
    *tx_prepare_status = payload[HEADER_CMD_SIZE + ACK_PREPARE_TX__STATUS];

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_PREPARE_TX\n");
    DEBUG_PRINTF("   id:           0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:         %u\n", cmd_get_size(payload));
    DEBUG_PRINTF("   status:       %u\n", *tx_prepare_status);
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_tx_status(const uint8_t * payload, e_tx_msg_status * tx_status) {
    /* sanity checks */
    if (payload == NULL) {
        printf("ERROR: invalid buffer\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_GET_TX_STATUS) {
        printf("ERROR: wrong type for EVT_TX_STATUS (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_GET_TX_STATUS, cmd_get_type(payload));
        return -1;
    }

    /* payload info */
    *tx_status = payload[HEADER_CMD_SIZE + ACK_GET_TX_STATUS__STATUS];

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_TX_STATUS\n");
    DEBUG_PRINTF("   id:           0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:         %u\n", cmd_get_size(payload));
    switch (*tx_status) {
        case TX_STATUS__IDLE:
            DEBUG_MSG("   status:       IDLE\n");
            break;
        case TX_STATUS__LOADED:
            DEBUG_MSG("   status:       LOADED\n");
            break;
        case TX_STATUS__ON_AIR:
            DEBUG_MSG("   status:       ON_AIR\n");
            break;
        case TX_STATUS__DONE:
            DEBUG_MSG("   status:       DONE\n");
            break;
        case TX_STATUS__ERROR_PARAM:
            DEBUG_MSG("   status:       ERROR_PARAM\n");
            break;
        case TX_STATUS__ERROR_FAIL_TO_SEND:
            DEBUG_MSG("   status:       ERROR_FAIL_TO_SEND\n");
            break;
        case TX_STATUS__ERROR_TX_TIMEOUT:
            DEBUG_MSG("   status:       ERROR_TX_TIMEOUT\n");
            break;
        default:
            DEBUG_PRINTF("   status:       UNKNOWN ?? (0x%02X)\n", *tx_status);
            return -1;
    }
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_config_rx(const uint8_t * payload, e_config_rx_status * config_rx_status) {
    /* sanity checks */
    if ((payload == NULL) || (config_rx_status == NULL)) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_CONFIG_RX) {
        printf("ERROR: wrong ACK type for CONFIG_RX (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_CONFIG_RX, cmd_get_type(payload));
        return -1;
    }

    /* payload info */
    *config_rx_status = payload[HEADER_CMD_SIZE + ACK_CONFIG_RX__STATUS];

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_CONFIG_RX\n");
    DEBUG_PRINTF("   id:           0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:         %u\n", cmd_get_size(payload));
    DEBUG_PRINTF("   status:       %u\n", *config_rx_status);
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_get_rx_msg(const uint8_t * payload, s_rx_msg * rx_msg) {
    /* sanity checks */
    if ((payload == NULL) || (rx_msg == NULL)) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_GET_RX_MSG) {
        printf("ERROR: wrong ACK type for GET_RX_MSG (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_GET_RX_MSG, cmd_get_type(payload));
        return -1;
    }

    /* payload info */
    rx_msg->nb_msg = payload[HEADER_CMD_SIZE + ACK_GET_RX_MSG__NB_MSG];

    rx_msg->nb_bytes  = payload[HEADER_CMD_SIZE + ACK_GET_RX_MSG__NB_BYTES_15_8] << 8;
    rx_msg->nb_bytes |= payload[HEADER_CMD_SIZE + ACK_GET_RX_MSG__NB_BYTES_7_0]  << 0;

    rx_msg->pending = payload[HEADER_CMD_SIZE + ACK_GET_RX_MSG__MSG_PENDING];

    rx_msg->lost_message = payload[HEADER_CMD_SIZE + ACK_GET_RX_MSG__LOST_MESSAGE];

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_GET_RX_MSG\n");
    DEBUG_PRINTF("   id:           0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:         %u\n", cmd_get_size(payload));
    DEBUG_PRINTF("   nb_msg:       %u\n", rx_msg->nb_msg);
    DEBUG_PRINTF("   nb_bytes:     %u\n", rx_msg->nb_bytes);
    DEBUG_PRINTF("   pending:      %u\n", rx_msg->pending);
    DEBUG_PRINTF("   lost_message: %u\n", rx_msg->lost_message);
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_evt_msg_received(const uint8_t * payload, struct lgw_pkt_rx_s * pkt) {
    /* sanity checks */
    if (payload == NULL) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__EVT_MSG_RECEIVE) {
        printf("ERROR: wrong ACK type for EVT_MSG_RECEIVED (expected:0x%02X, got 0x%02X)\n", ORDER_ID__EVT_MSG_RECEIVE, cmd_get_type(payload));
        return -1;
    }

    /* payload info */
    memset(pkt, 0, sizeof (struct lgw_pkt_rx_s));
    pkt->channel = payload[HEADER_CMD_SIZE + EVT_MSG_RECEIVE__RADIO_IDX];
    pkt->count_us = bytes_be_to_uint32_le(&payload[HEADER_CMD_SIZE + EVT_MSG_RECEIVE__TIMESTAMP_31_24]);
    pkt->foff_hz = bytes_be_to_int32_le(&payload[HEADER_CMD_SIZE + EVT_MSG_RECEIVE__ERROR_FREQ_31_24]);
    pkt->snr = (float)((int8_t)payload[HEADER_CMD_SIZE + EVT_MSG_RECEIVE__SNR]);
    pkt->rssi = (float)((int8_t)payload[HEADER_CMD_SIZE + EVT_MSG_RECEIVE__RSSI]);
    pkt->size = payload[HEADER_CMD_SIZE + EVT_MSG_RECEIVE__PAYLOAD_LEN];
    memcpy(pkt->payload, payload + HEADER_CMD_SIZE + EVT_MSG_RECEIVE__PAYLOAD, pkt->size);

#if DEBUG_VERBOSE
    int i;

    DEBUG_MSG   ("## EVT_MSG_RECEIVED\n");
    DEBUG_PRINTF("   chan:      %u\n", pkt->channel);
    DEBUG_PRINTF("   count_us:  %u\n", pkt->count_us);
    DEBUG_PRINTF("   snr:       %.1f\n", pkt->snr);
    DEBUG_PRINTF("   rssi:      %.1f\n", pkt->rssi);
    DEBUG_PRINTF("   size:      %u\n", pkt->size);
    DEBUG_MSG   ("   data:      ");
    for (i = 0; i < pkt->size; i++) {
        DEBUG_PRINTF("%02X", pkt->payload[i]);
    }
    DEBUG_MSG   ("\n");
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_reset(const uint8_t * payload, uint8_t * reset_status) {
     /* sanity checks */
    if (payload == NULL) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_RESET) {
        printf("ERROR: wrong ACK type for ACK_RESET (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_RESET, cmd_get_type(payload));
        return -1;
    }

    /* payload info */
    *reset_status = payload[HEADER_CMD_SIZE + ACK_RESET__STATUS];

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_RESET\n");
    DEBUG_PRINTF("   id:           0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:         %u\n", cmd_get_size(payload));
    DEBUG_PRINTF("   status:       %u\n", *reset_status);
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_bootloader_mode(const uint8_t * payload) {
     /* sanity checks */
    if (payload == NULL) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_BOOTLOADER_MODE) {
        printf("ERROR: wrong ACK type for ACK_BOOTLOADER_MODE (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_BOOTLOADER_MODE, cmd_get_type(payload));
        return -1;
    }

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_BOOTLOADER_MODE\n");
    DEBUG_PRINTF("   id:           0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:         %u\n", cmd_get_size(payload));
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_read_register(const uint8_t * payload, uint8_t * reg_value) {
     /* sanity checks */
    if (payload == NULL) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_READ_REGS) {
        printf("ERROR: wrong ACK type for ACK_READ_REGS (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_READ_REGS, cmd_get_type(payload));
        return -1;
    }

    /* payload info */
    *reg_value = payload[HEADER_CMD_SIZE + ACK_READ_REG__VALUE];

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_READ_REG\n");
    DEBUG_PRINTF("   id:           0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:         %u\n", cmd_get_size(payload));
    DEBUG_PRINTF("   value:        %u\n", *reg_value);
#endif

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int decode_ack_write_register(const uint8_t * payload) {
     /* sanity checks */
    if (payload == NULL) {
        printf("ERROR: invalid parameter\n");
        return -1;
    }

    if (cmd_get_type(payload) != ORDER_ID__ACK_WRITE_REGS) {
        printf("ERROR: wrong ACK type for ACK_WRITE_REGS (expected:0x%02X, got 0x%02X)\n", ORDER_ID__ACK_WRITE_REGS, cmd_get_type(payload));
        return -1;
    }

#if DEBUG_VERBOSE
    DEBUG_MSG   ("## ACK_WRITE_REG\n");
    DEBUG_PRINTF("   id:           0x%02X\n", cmd_get_id(payload));
    DEBUG_PRINTF("   size:         %u\n", cmd_get_size(payload));
#endif

    return 0;
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int mcu_open(const char * tty_path) {
    int fd;
    struct termios tty;

    fd = open(tty_path, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd == -1) {
        perror("ERROR: Unable to open tty_path - ");
    } else {
        memset(&tty, 0, sizeof tty);

        /* Get current attributes */
        if (tcgetattr(fd, &tty) != 0) {
            printf("ERROR: tcgetattr failed with %d - %s", errno, strerror(errno));
            return -1;
        }

        cfsetospeed(&tty, 115200);
        cfsetispeed(&tty, 115200);

        /* Control Modes */
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; /* set 8-bit characters */
        tty.c_cflag |= CLOCAL;                      /* local connection, no modem control */
        tty.c_cflag |= CREAD;                       /* enable receiving characters */
        tty.c_cflag &= ~PARENB;                     /* no parity */
        tty.c_cflag &= ~CSTOPB;                     /* one stop bit */
        /* Input Modes */
        tty.c_iflag &= ~IGNBRK;
        tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
        /* Output Modes */
        tty.c_oflag &= ~IGNBRK;
        tty.c_oflag &= ~(IXON | IXOFF | IXANY | ICRNL);
        /* Local Modes */
        tty.c_lflag = 0;
        /* Settings for non-canonical mode */
        /* set blocking mode, need at least n char to return */
        tty.c_cc[VMIN] = HEADER_CMD_SIZE;   /* x bytes minimum for full message header */
        tty.c_cc[VTIME] = 1;                /* 100ms */

        /* set attributes */
        if (tcsetattr(fd, TCSANOW, &tty) != 0) {
            DEBUG_PRINTF("ERROR: tcsetattr(TCSANOW) failed with %d - %s", errno, strerror(errno));
            return -1;
        }

        /* flush input/ouput queues */
        wait_ms(100);
        if (tcflush(fd, TCIOFLUSH) != 0) {
            DEBUG_PRINTF("ERROR: tcflush failed with %d - %s", errno, strerror(errno));
            return -1;
        }
    }

    /* Initialize pseudo-randoml generator for request ID */
    srand(0);

    return fd;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_close(int fd) {
    return close(fd);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_get_status(int fd, s_status * status) {
    CHECK_NULL(status);

    if (write_req(fd, ORDER_ID__REQ_GET_STATUS, 0, NULL) != 0) {
        printf("ERROR: failed to write GET_STATUS request\n");
        return -1;
    }

    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read GET_STATUS ack\n");
        return -1;
    }

    if (decode_ack_get_status(buf_ack, status) != 0) {
        printf("ERROR: invalid GET_STATUS ack\n");
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_get_tx_status(int fd, e_tx_msg_status * status) {
    CHECK_NULL(status);

    if (write_req(fd, ORDER_ID__REQ_GET_TX_STATUS, 0, NULL) != 0) {
        printf("ERROR: failed to write GET_TX_STATUS request\n");
        return -1;
    }

    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read GET_TX_STATUS ack\n");
        return -1;
    }

    if (decode_ack_tx_status(buf_ack, status) != 0) {
        printf("ERROR: failed to decode GET_TX_STATUS ack\n");
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_ping(int fd, s_ping_info * info) {
    CHECK_NULL(info);

    if (write_req(fd, ORDER_ID__REQ_PING, 0, NULL) != 0) {
        printf("ERROR: failed to write PING request\n");
        return -1;
    }

    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read PING ack\n");
        return -1;
    }

    if (decode_ack_ping(buf_ack, info) != 0) {
        printf("ERROR: invalid PING ack\n");
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_prepare_tx(int fd, const struct lgw_pkt_tx_s * pkt_data, bool blocking) {
    e_prepare_tx_status tx_prepare_status;
    e_tx_msg_status tx_status;
    bool tx_complete = false;

    /* Check params */
    if (pkt_data == NULL) {
        return -1;
    }
    if (nb_radio_rx < 1) {
        printf("ERROR: cannot prepare tx, no radio available\n");
        return -1;
    }

    /* Trigger type */
    buf_req[REQ_PREPARE_TX__MSG_IS_TIMESTAMP] = (uint8_t)pkt_data->tx_mode;

    /* Timestamp */
    buf_req[REQ_PREPARE_TX__TIMESTAMP_31_24] = (uint8_t)(pkt_data->count_us >> 24);
    buf_req[REQ_PREPARE_TX__TIMESTAMP_23_16] = (uint8_t)(pkt_data->count_us >> 16);
    buf_req[REQ_PREPARE_TX__TIMESTAMP_15_8]  = (uint8_t)(pkt_data->count_us >> 8);
    buf_req[REQ_PREPARE_TX__TIMESTAMP_7_0]   = (uint8_t)(pkt_data->count_us >> 0);

    /* Power */
    buf_req[REQ_PREPARE_TX__POWER] = pkt_data->rf_power;

    /* Frequency */
    buf_req[REQ_PREPARE_TX__FREQ_31_24] = (uint8_t)(pkt_data->freq_hz >> 24);
    buf_req[REQ_PREPARE_TX__FREQ_23_16] = (uint8_t)(pkt_data->freq_hz >> 16);
    buf_req[REQ_PREPARE_TX__FREQ_15_8]  = (uint8_t)(pkt_data->freq_hz >> 8);
    buf_req[REQ_PREPARE_TX__FREQ_7_0]   = (uint8_t)(pkt_data->freq_hz >> 0);

    /* Bandwidth */
    buf_req[REQ_PREPARE_TX__BW] = (uint8_t)(pkt_data->bandwidth);

    /* SF */
    buf_req[REQ_PREPARE_TX__SF] = (uint8_t)(pkt_data->datarate);

    /* Polarity */
    buf_req[REQ_PREPARE_TX__USE_INVERSE_IQ] = (pkt_data->invert_pol == false) ? 0 : 1;

    /* Coding rate */
    switch (pkt_data->coderate) {
        case CR_LORA_4_5:
            buf_req[REQ_PREPARE_TX__CR] = 0;
            break;
        case CR_LORA_4_6:
            buf_req[REQ_PREPARE_TX__CR] = 1;
            break;
        case CR_LORA_4_7:
            buf_req[REQ_PREPARE_TX__CR] = 2;
            break;
        case CR_LORA_4_8:
            buf_req[REQ_PREPARE_TX__CR] = 3;
            break;
        case CR_LORA_LI_4_5:
            buf_req[REQ_PREPARE_TX__CR] = 4;
            break;
        case CR_LORA_LI_4_6:
            buf_req[REQ_PREPARE_TX__CR] = 5;
            break;
        case CR_LORA_LI_4_7:
            buf_req[REQ_PREPARE_TX__CR] = 6;
            break;
        default:
            printf("ERROR: invalid coding rate\n");
            return -1;
    }

    /* CRC */
    buf_req[REQ_PREPARE_TX__USE_IMPLICIT_HEADER] = (pkt_data->no_header == true) ? 1 : 0;

    /* Implicit/Explicit Header */
    buf_req[REQ_PREPARE_TX__USE_CRC] = (pkt_data->no_crc == false) ? 1 : 0;

    /* Radio Ramp time */
    buf_req[REQ_PREPARE_TX__RAMP_UP] = RADIO_RAMP_20_US;

    /* Preamble length */
    buf_req[REQ_PREPARE_TX__PREAMBLE_15_8]  = (uint8_t)(pkt_data->preamble >> 8);
    buf_req[REQ_PREPARE_TX__PREAMBLE_7_0]   = (uint8_t)(pkt_data->preamble >> 0);

    /* Payload length */
    buf_req[REQ_PREPARE_TX__PAYLOAD_LEN] = pkt_data->size;

    /* Payload */
    memcpy(&buf_req[REQ_PREPARE_TX__PAYLOAD], pkt_data->payload, pkt_data->size);

    /* Send TX request */
    if (write_req(fd, ORDER_ID__REQ_PREPARE_TX, (uint16_t)REQ_PREPARE_TX__PAYLOAD + pkt_data->size, buf_req) != 0) {
        printf("ERROR: failed to write PREPARE_TX request\n");
        return -1;
    }

    /* Wait for ACK */
    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read PREPARE_TX ack\n");
        return -1;
    }

    if (decode_ack_prepare_tx(buf_ack, &tx_prepare_status) != 0) {
        printf("ERROR: invalid PREPARE_TX ack\n");
        return -1;
    }

    if (tx_prepare_status != PREPARE_TX_STATUS__OK) {
        printf("ERROR: PREPARE_TX rejected with 0x%02X\n", tx_prepare_status);
        return -1;
    }

    /* Wait for TX to be done if requested */
    if (blocking == true) {
        do {
            if (write_req(fd, ORDER_ID__REQ_GET_TX_STATUS, 0, NULL) != 0) {
                printf("ERROR: failed to write GET_TX_STATUS request\n");
                return -1;
            }

            if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
                printf("ERROR: failed to read GET_TX_STATUS ack\n");
                return -1;
            }

            if (decode_ack_tx_status(buf_ack, &tx_status) != 0) {
                printf("ERROR: failed to decode GET_TX_STATUS ack\n");
                return -1;
            }

            tx_complete =  ((tx_status == TX_STATUS__IDLE) ||
                            (tx_status == TX_STATUS__ERROR_PARAM) ||
                            (tx_status == TX_STATUS__ERROR_FAIL_TO_SEND) ||
                            (tx_status == TX_STATUS__ERROR_TX_TIMEOUT));
            if (tx_complete == false) {
                wait_ms(10);
            }
        } while (tx_complete == false);
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_config_rx(int fd, uint8_t channel, const struct lgw_conf_channel_rx_s * conf) {
    e_config_rx_status config_rx_status;

    /* Check params */
    if (conf == NULL) {
        return -1;
    }
    if (channel >= nb_radio_rx) {
        printf("ERROR: cannot configure channel %u, not enough radios available (%u)\n", channel, nb_radio_rx);
        return -1;
    }

    buf_req[REQ_CONF_RX__RADIO_IDX] = channel;

    buf_req[REQ_CONF_RX__FREQ_31_24] = (uint8_t)(conf->freq_hz >> 24);
    buf_req[REQ_CONF_RX__FREQ_23_16] = (uint8_t)(conf->freq_hz >> 16);
    buf_req[REQ_CONF_RX__FREQ_15_8]  = (uint8_t)(conf->freq_hz >> 8);
    buf_req[REQ_CONF_RX__FREQ_7_0]   = (uint8_t)(conf->freq_hz >> 0);

    buf_req[REQ_CONF_RX__PREAMBLE_LEN_15_8] = (uint8_t)(STD_LORA_PREAMBLE >> 8);
    buf_req[REQ_CONF_RX__PREAMBLE_LEN_7_0]  = (uint8_t)(STD_LORA_PREAMBLE >> 0);

    buf_req[REQ_CONF_RX__SF] = (uint8_t)(conf->datarate);

    buf_req[REQ_CONF_RX__BW] = (uint8_t)(conf->bandwidth);

    buf_req[REQ_CONF_RX__USE_IQ_INVERTED] = 0;

    /* Send CONFIG_RX request */
    if (write_req(fd, ORDER_ID__REQ_CONFIG_RX, REQ_CONF_RX_SIZE, buf_req) != 0) {
        printf("ERROR: failed to write CONFIG_RX request\n");
        return -1;
    }

    /* Wait for ACK */
    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read CONFIG_RX ack\n");
        return -1;
    }

    if (decode_ack_config_rx(buf_ack, &config_rx_status) != 0) {
        printf("ERROR: invalid CONFIG_RX ack\n");
        return -1;
    }

    if (config_rx_status != CONFIG_RX_SATUS__DONE) {
        printf("ERROR: CONFIG_RX rejected with 0x%02X\n", config_rx_status);
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_receive(int fd, uint8_t max_pkt, struct lgw_pkt_rx_s * pkt, uint8_t * nb_pkt) {
    s_rx_msg rx_msg;
    int i;
    struct lgw_pkt_rx_s * p;

    /* Check params */
    CHECK_NULL(pkt)
    CHECK_NULL(nb_pkt);

    *nb_pkt = 0;

    /* Check if there are packets received */
    if (write_req(fd, ORDER_ID__REQ_GET_RX_MSG, 0, NULL) != 0) {
        printf("ERROR: failed to write GET_RX_MSG request\n");
        return -1;
    }

    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read GET_RX_MSG ack\n");
        return -1;
    }

    if (decode_ack_get_rx_msg(buf_ack, &rx_msg) != 0) {
        printf("ERROR: invalid GET_RX_MSG ack\n");
        return -1;
    }

    if (rx_msg.lost_message > 0) {
        printf("WARNING: %u packets lost\n", rx_msg.lost_message);
    }

    /* Get packets one by one */
    for (i = 0; i < rx_msg.nb_msg; i++) {
        if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
            printf("ERROR: failed to read EVT_MSG_RECEIVED\n");
            return -1;
        }

        /* Drop packets that cannot be stored in given buffer */
        if (i >= max_pkt) {
            printf("WARNING: dropping packet, not enough room in buffer to store it\n");
            continue;
        }

        /* Store packet in given array */
        p = &pkt[i];
        if (decode_evt_msg_received(buf_ack, p) != 0) {
            printf("ERROR: invalid EVT_MSG_RECEIVED evt\n");
            return -1;
        }

        *nb_pkt += 1;
    }

    if (rx_msg.pending != 0) {
        printf("INFO: there are pending messages\n"); /* TODO: let the application call back to get the pending packets or automatically do it here ? */
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_reset(int fd, bool reset_mcu) {
    uint8_t status;

    /* Reset All RX radios */
    buf_req[REQ_RESET__TYPE] = RESET_TYPE__RX_ALL;
    if (write_req(fd, ORDER_ID__REQ_RESET, REQ_RESET_SIZE, buf_req) != 0) {
        printf("ERROR: failed to write RESET request\n");
        return -1;
    }

    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read RESET ack\n");
        return -1;
    }

    if (decode_ack_reset(buf_ack, &status) != 0) {
        printf("ERROR: invalid RESET ack\n");
        return -1;
    }

    if (status != 0) {
        printf("ERROR: Failed to reset RX radios\n");
        return -1;
    }

    /* Reset TX radio */
    buf_req[REQ_RESET__TYPE] = RESET_TYPE__TX;
    if (write_req(fd, ORDER_ID__REQ_RESET, REQ_RESET_SIZE, buf_req) != 0) {
        printf("ERROR: failed to write RESET request\n");
        return -1;
    }

    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read RESET ack\n");
        return -1;
    }

    if (decode_ack_reset(buf_ack, &status) != 0) {
        printf("ERROR: invalid RESET ack\n");
        return -1;
    }

    if (status != 0) {
        printf("ERROR: Failed to reset TX radios\n");
        return -1;
    }

    /* Reset MCU */
    if (reset_mcu == true) {
        buf_req[REQ_RESET__TYPE] = RESET_TYPE__GTW;
        if (write_req(fd, ORDER_ID__REQ_RESET, REQ_RESET_SIZE, buf_req) != 0) {
            printf("ERROR: failed to write RESET request\n");
            return -1;
        }

        if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
            printf("ERROR: failed to read RESET ack\n");
            return -1;
        }

        if (decode_ack_reset(buf_ack, &status) != 0) {
            printf("ERROR: invalid RESET ack\n");
            return -1;
        }

        if (status != 0) {
            printf("ERROR: Failed to reset concentrator MCU\n");
            return -1;
        }
    }

    /* Wait for MCU to get ready after reset */
    wait_ms(500);

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_boot(int fd) {
    if (write_req(fd, ORDER_ID__REQ_BOOTLOADER_MODE, 0, NULL) != 0) {
        printf("ERROR: failed to write BOOTLOADER_MODE request\n");
        return -1;
    }

    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read BOOTLOADER_MODE ack\n");
        return -1;
    }

    if (decode_ack_bootloader_mode(buf_ack) != 0) {
        printf("ERROR: invalid BOOTLOADER_MODE ack\n");
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_read_register(int fd, uint8_t radio_idx, uint16_t addr, uint8_t * value) {
    CHECK_NULL(value);

    buf_req[REQ_READ_REGS__RADIO_IDX] = radio_idx;
    buf_req[REQ_READ_REGS__ADDR_15_8] = (uint8_t)(addr >> 8);
    buf_req[REQ_READ_REGS__ADDR_7_0]  = (uint8_t)(addr >> 0);

    if (write_req(fd, ORDER_ID__REQ_READ_REGS, REQ_READ_REGS_SIZE, buf_req) != 0) {
        printf("ERROR: failed to write REQ_READ_REGS request\n");
        return -1;
    }

    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read REQ_READ_REGS ack\n");
        return -1;
    }

    if (decode_ack_read_register(buf_ack, value) != 0) {
        printf("ERROR: invalid REQ_READ_REGS ack\n");
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int mcu_write_register(int fd, uint8_t radio_idx, uint16_t addr, const uint8_t value) {
    buf_req[REQ_WRITE_REGS__RADIO_IDX] = radio_idx;
    buf_req[REQ_WRITE_REGS__ADDR_15_8] = (uint8_t)(addr >> 8);
    buf_req[REQ_WRITE_REGS__ADDR_7_0]  = (uint8_t)(addr >> 0);
    buf_req[REQ_WRITE_REGS__DATA] = value;

    if (write_req(fd, ORDER_ID__REQ_WRITE_REGS, REQ_WRITE_REGS_SIZE, buf_req) != 0) {
        printf("ERROR: failed to write REQ_WRITE_REGS request\n");
        return -1;
    }

    if (read_ack(fd, buf_ack, sizeof buf_ack) < 0) {
        printf("ERROR: failed to read REQ_WRITE_REGS ack\n");
        return -1;
    }

    if (decode_ack_write_register(buf_ack) != 0) {
        printf("ERROR: invalid REQ_WRITE_REGS ack\n");
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint8_t mcu_get_nb_rx_radio(void) {
    return nb_radio_rx;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint8_t mcu_get_nb_tx_radio(void) {
    return nb_radio_tx;
}

/* --- EOF ------------------------------------------------------------------ */