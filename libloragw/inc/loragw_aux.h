/*!
 * \brief     LoRa 2.4GHz concentrator HAL common auxiliary functions
 *
 * License: Revised BSD 3-Clause License, see LICENSE.TXT file include in the project
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
