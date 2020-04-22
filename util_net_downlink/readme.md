	  ______                              _
	 / _____)             _              | |
	( (____  _____ ____ _| |_ _____  ____| |__
	 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
	 _____) ) ____| | | || |_| ____( (___| | | |
	(______/|_____)_|_|_| \__)_____)\____)_| |_|
	  (C)2020 Semtech

Utility: UDP Downlink server / Packet Logger
============================================

## 1. Introduction

This utility allows to send regular downlink requests to the packet forwarder
running on the gateway.

The downlinks are sent in 'immediate' mode, meaning that the concentrator will
send the incoming packet over the air as soon as it receives it.

The net_downlink utility will construct a JSON 'txpk' object based on given
command line arguments, and send it on a UDP socket on the given port. Then the
packet forwarder receives it on its downlink socket, parses the JSON object to
build the packet buffer to be sent to the concentrator board.

This utility can be compiled and run on the gateway itself, or on a PC.

Optionally, the net_downlink utility can forward the received uplinks
(PUSH_DATA) to another UDP server, which can be useful for uplink Packet Error
Rate measurement while performing downlink testing (full-duplex testing etc...)

In can also be used as a UDP packet logger, logging all uplinks in a CSV file.

## 2. Dependencies

A packet forwarder must be running to receive downlink packets and send it to
the concentrator board.

## 3. Usage

### 3.1. Packet Forwarder configuration

The 'global_conf.json' file provided with the packet forwarder can be used, only
the 'server_address' must be set to 'localhost' if net_downlink is running on
the gateway itself, or set to the IP address of the PC on which the utility is
running.

### 3.2. Launching the packet forwarder

The packet forwarder has to be launched with the global_conf.json described in
3.1.

 `./lora_pkt_fwd -c global_conf.json`

### 3.3. Launching net_downlink

The net_downlink utility can be started with various command line arguments.

In order to get the available options, and some examples, run:

`./net_downlink -h`

To stop the application, press Ctrl+C.

## 4. License

--- Revised 3-Clause BSD License ---
Copyright (C) 2020, SEMTECH (International) AG.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  * Neither the name of the Semtech nor the names of its contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL SEMTECH BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.