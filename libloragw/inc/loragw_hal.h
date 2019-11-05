/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    LoRa 2.4GHz concentrator Hardware Abstraction Layer

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


#ifndef _LORAGW_HAL_H
#define _LORAGW_HAL_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */

#include "config.h"     /* library configuration options (dynamically generated) */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC MACROS -------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

/* return status code */
#define LGW_HAL_SUCCESS     0
#define LGW_HAL_ERROR       -1

/* radio parameters */
#define LGW_RX_CHANNEL_NB_MAX 3    /* Maximum number of RX channels supported */
#define LGW_TX_CHANNEL_NB_MAX 1    /* Maximum number of RX channels supported */

/* modulation parameters */
#define STD_LORA_PREAMBLE   8
#define MIN_LORA_PREAMBLE   8

#define TX_POWER_MIN        -18 /* dBm */
#define TX_POWER_MAX        13  /* dBm */
#define TX_POWER_DEFAULT    10  /* dBm */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

typedef enum {
    TIMESTAMPED,
    IMMEDIATE,
    ON_GPS,
    CW_ON,
    CW_OFF
} e_tx_mode;

typedef enum {
    BW_200KHZ  = 8,
    BW_400KHZ  = 10,
    BW_800KHZ  = 12,
    BW_1600KHZ = 13
} e_bandwidth;

typedef enum {
    DR_LORA_SF5 = 5,
    DR_LORA_SF6,
    DR_LORA_SF7,
    DR_LORA_SF8,
    DR_LORA_SF9,
    DR_LORA_SF10,
    DR_LORA_SF11,
    DR_LORA_SF12
} e_spreading_factor;

typedef enum {
    CR_LORA_4_5    = 0x01,
    CR_LORA_4_6    = 0x02,
    CR_LORA_4_7    = 0x03,
    CR_LORA_4_8    = 0x04,
    CR_LORA_LI_4_5 = 0x05,
    CR_LORA_LI_4_6 = 0x06,
    CR_LORA_LI_4_7 = 0x07
} e_coding_rate;

typedef enum {
    MOD_LORA
} e_modulation;

typedef enum {
    TX_STATUS,
    RX_STATUS
} e_status_type;

typedef enum {
    TX_STATUS_UNKNOWN,
    TX_OFF,
    TX_FREE,
    TX_SCHEDULED,
    TX_EMITTING,
    RX_STATUS_UNKNOWN,
    RX_OFF,
    RX_ON,
    RX_SUSPENDED
} e_status;

typedef enum {
    STAT_UNDEFINED  = 0x00,
    STAT_NO_CRC     = 0x01,
    STAT_CRC_BAD    = 0x11,
    STAT_CRC_OK     = 0x10
} e_crc_status;

/**
@struct lgw_conf_board_s
@brief Configuration structure for board specificities
*/
struct lgw_conf_board_s {
    char tty_path[64];/*!> Path to access the TTY device to connect to concentrator board */
};

/**
@struct lgw_conf_channel_rx_s
@brief Configuration structure for a channel
*/
struct lgw_conf_channel_rx_s {
    bool                enable;         /*!> enable or disable that channel */
    uint32_t            freq_hz;        /*!> channel frequency in Hz */
    e_bandwidth         bandwidth;      /*!> RX bandwidth */
    e_spreading_factor  datarate;       /*!> RX datarate */
    float               rssi_offset;    /*!> RSSI offset to be applied on this channel */
};

/**
@struct lgw_conf_channel_tx_s
@brief Configuration structure for TX
*/
struct lgw_conf_channel_tx_s {
    bool                enable;         /*!> enable or disable that channel */
};

/**
@struct lgw_pkt_tx_s
@brief Structure containing the configuration of a packet to send and a pointer to the payload
*/
struct lgw_pkt_tx_s {
    uint32_t            freq_hz;        /*!> center frequency of TX */
    e_tx_mode           tx_mode;        /*!> select on what event/time the TX is triggered */
    uint32_t            count_us;       /*!> timestamp or delay in microseconds for TX trigger */
    int8_t              rf_power;       /*!> TX power, in dBm */
    e_bandwidth         bandwidth;      /*!> modulation bandwidth (LoRa only) */
    e_spreading_factor  datarate;       /*!> TX datarate (SF for LoRa) */
    e_coding_rate       coderate;       /*!> error-correcting code of the packet (LoRa only) */
    bool                invert_pol;     /*!> invert signal polarity, for orthogonal downlinks (LoRa only) */
    uint16_t            preamble;       /*!> set the preamble length, 0 for default */
    bool                no_crc;         /*!> if true, do not send a CRC in the packet */
    bool                no_header;      /*!> if true, enable implicit header mode (LoRa) */
    uint16_t            size;           /*!> payload size in bytes */
    uint8_t             payload[256];   /*!> buffer containing the payload */
};

/**
@struct lgw_pkt_rx_s
@brief Structure containing the metadata of a packet that was received and a pointer to the payload
*/
struct lgw_pkt_rx_s {
    uint32_t            freq_hz;        /*!> central frequency of the IF chain */
    uint8_t             channel;        /*!> by which IF chain was packet received */
    uint8_t             status;         /*!> status of the received packet */
    uint32_t            count_us;       /*!> internal concentrator counter for timestamping, 1 microsecond resolution */
    int32_t             foff_hz;        /*!> frequency error in Hz */
    e_modulation        modulation;     /*!> modulation used by the packet */
    e_bandwidth         bandwidth;      /*!> modulation bandwidth (LoRa only) */
    e_spreading_factor  datarate;       /*!> RX datarate of the packet (SF for LoRa) */
    e_coding_rate       coderate;       /*!> error-correcting code of the packet (LoRa only) */
    float               rssi;           /*!> average packet RSSI in dB */
    float               snr;            /*!> average packet SNR, in dB (LoRa only) */
    uint16_t            size;           /*!> payload size in bytes */
    uint8_t             payload[256];   /*!> buffer containing the payload */
};

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief Configure the gateway board
@param conf structure containing the configuration parameters
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_board_setconf(const struct lgw_conf_board_s * conf);

/**
@brief Configure a RX channel
@param index of the radio handling the channel to configure [0, LGW_RX_CHANNEL_NB_MAX - 1]
@param conf structure containing the configuration parameters
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_channel_rx_setconf(uint8_t channel, const struct lgw_conf_channel_rx_s * conf);

/**
@brief Configure TX
@param conf structure containing the configuration parameters
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_channel_tx_setconf(const struct lgw_conf_channel_tx_s * conf);

/**
@brief Connect to the LoRa concentrator, reset it and configure it according to previously set parameters
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_start(void);

/**
@brief Stop the LoRa concentrator and disconnect it
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_stop(void);

/**
@brief A non-blocking function that will fetch up to 'max_pkt' packets from the LoRa concentrator FIFO and data buffer
@param max_pkt maximum number of packet that must be retrieved (equal to the size of the array of struct)
@param pkt_data pointer to an array of struct that will receive the packet metadata and payload pointers
@return LGW_HAL_ERROR id the operation failed, else the number of packets retrieved
*/
int lgw_receive(uint8_t max_pkt, struct lgw_pkt_rx_s * pkt_data);

/**
@brief Schedule a packet to be send immediately or after a delay depending on tx_mode
@param pkt_data structure containing the data and metadata for the packet to send
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else

/!\ When sending a packet, there is a delay (approx 1.5ms) for the analog
circuitry to start and be stable. This delay is adjusted by the HAL depending
on the board version (lgw_i_tx_start_delay_us).

In 'timestamp' mode, this is transparent: the modem is started
lgw_i_tx_start_delay_us microseconds before the user-set timestamp value is
reached, the preamble of the packet start right when the internal timestamp
counter reach target value.

In 'immediate' mode, the packet is emitted as soon as possible: transferring the
packet (and its parameters) from the host to the concentrator takes some time,
then there is the lgw_i_tx_start_delay_us, then the packet is emitted.

In 'triggered' mode (aka PPS/GPS mode), the packet, typically a beacon, is
emitted lgw_i_tx_start_delay_us microsenconds after a rising edge of the
trigger signal. Because there is no way to anticipate the triggering event and
start the analog circuitry beforehand, that delay must be taken into account in
the protocol.
*/
int lgw_send(const struct lgw_pkt_tx_s * pkt_data);

/**
@brief Give the the status of different part of the LoRa concentrator
@param select is used to select what status we want to know
@param code is used to return the status code
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_status(e_status_type select, e_status * code);

/**
@brief Abort a currently scheduled or ongoing TX
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_abort_tx(void);

/**
@brief Return value of internal counter when latest event (eg GPS pulse) was captured
@param trig_cnt_us pointer to receive timestamp value
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_get_trigcnt(uint32_t * trig_cnt_us);

/**
@brief Return instateneous value of internal counter
@param inst_cnt_us pointer to receive timestamp value
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_get_instcnt(uint32_t * inst_cnt_us);

/**
@brief Allow user to check the version/options of the library once compiled
@return pointer on a human-readable null terminated string
*/
const char* lgw_version_info(void);

/**
@brief Return the LoRa concentrator EUI
@param eui pointer to receive eui
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_get_eui(uint64_t * eui);

/**
@brief Return the temperature measured by the LoRa concentrator sensor (updated every 30s)
@param temperature The temperature measured, in degree celcius
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_get_temperature(float * temperature);

/**
@brief Return time on air of given packet, in milliseconds
@param packet is a pointer to the packet structure
@return the packet time on air in milliseconds
*/
uint32_t lgw_time_on_air(const struct lgw_pkt_tx_s * pkt, double * result);

/**
 */
uint16_t lgw_get_bw_khz(e_bandwidth bandwidth);

#endif

/* --- EOF ------------------------------------------------------------------ */