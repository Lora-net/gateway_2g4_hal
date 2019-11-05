/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    LoRa 2.4GHz concentrator HAL common auxiliary functions

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

#ifndef _LORAGW_AUX_H
#define _LORAGW_AUX_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include "config.h"    /* library configuration options (dynamically generated) */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC MACROS -------------------------------------------------------- */

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

void wait_ms(unsigned long a);

uint32_t bytes_be_to_uint32_le(const uint8_t * bytes);
int32_t bytes_be_to_int32_le(const uint8_t * bytes);

#endif

/* --- EOF ------------------------------------------------------------------ */
