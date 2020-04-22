	  ______                              _
	 / _____)             _              | |
	( (____  _____ ____ _| |_ _____  ____| |__
	 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
	 _____) ) ____| | | || |_| ____( (___| | | |
	(______/|_____)_|_|_| \__)_____)\____)_| |_|
	  (C)2020 Semtech

Basic communication protocol between LoRa 2.4Ghz gateway and Network Server
===========================================================================


## 1. Introduction

The protocol between the gateway and the server is purposefully very basic and
for demonstration purpose only, or for use on private and reliable networks.

There is no authentication of the gateway or the server, and the acknowledges
are only used for network quality assessment, not to correct UDP datagrams
losses (no retries).


## 2. System schematic and definitions

	 ((( Y )))
	     |
	     |
	+ - -|- - - - - - - - - - - - - +        xxxxxxxxxxxx          +--------+
	| +--+-----------+     +------+ |       xx x  x     xxx        |        |
	| |              |     |      | |      xx  Internet  xx        |        |
	| | Concentrator |<--->| Host |<-------xx     or    xx-------->|        |
	| |              | USB |      | |      xx  Intranet  xx        | Server |
	| +--------------+     +------+ |       xxxx   x   xxxx        |        |
	|                               |           xxxxxxxx           |        |
	|             Gateway           |                              |        |
	+- - - - - - - - - - - - - - - -+                              +--------+

__Concentrator__: radio RX/TX board, based on Semtech stand-alone modems
(SX1280), with an interface MCU.

__Host__: embedded computer on which the packet forwarder is run. Drives the
concentrator through a USB link.

__Gateway__: a device composed of at least one radio concentrator, a host, some
network connection to the internet or a private network (Ethernet, 3G, Wifi,
microwave link).

__Server__: an abstract computer that will process the RF packets received and
forwarded by the gateway, and issue RF packets in response that the gateway
will have to emit.

It is assumed that the gateway can be behind a NAT or a firewall stopping any
incoming connection.
It is assumed that the server has an static IP address (or an address solvable
through a DNS service) and is able to receive incoming connections on a
specific port.


## 3. Upstream protocol

### 3.1. Sequence diagram ###

	+---------+                                                    +---------+
	| Gateway |                                                    | Server  |
	+---------+                                                    +---------+
	     | -----------------------------------\                         |
	     |-| When 1-N RF packets are received |                         |
	     | ------------------------------------                         |
	     |                                                              |
	     | PUSH_DATA (token X, GW MAC, JSON payload)                    |
	     |------------------------------------------------------------->|
	     |                                                              |
	     |                                           PUSH_ACK (token X) |
	     |<-------------------------------------------------------------|
	     |                              ------------------------------\ |
	     |                              | process packets *after* ack |-|
	     |                              ------------------------------- |
	     |                                                              |

### 3.2. PUSH_DATA packet ###

That packet type is used by the gateway mainly to forward the RF packets
received, and associated metadata, to the server.

 Bytes  | Function
:------:|---------------------------------------------------------------------
 0      | protocol version = 2
 1-2    | random token
 3      | PUSH_DATA identifier 0x00
 4-11   | Gateway unique identifier (MAC address)
 12-end | JSON object, starting with {, ending with }, see section 4

### 3.3. PUSH_ACK packet ###

That packet type is used by the server to acknowledge immediately all the
PUSH_DATA packets received.

 Bytes  | Function
:------:|---------------------------------------------------------------------
 0      | protocol version = 2
 1-2    | same token as the PUSH_DATA packet to acknowledge
 3      | PUSH_ACK identifier 0x01


## 4. Upstream JSON data structure

The root object can contain an array named "rxpk":

``` json
{
	"rxpk":[ {...}, ...]
}
```

That array contains at least one JSON object, each object contain a RF packet
and associated metadata with the following fields:

 Name |  Type  | Function
:----:|:------:|--------------------------------------------------------------
 tmst | number | Internal timestamp of "RX finished" event (32b unsigned)
 freq | number | RX central frequency in MHz (unsigned float, Hz precision)
 foff | number | Frequency offset in Hz (32b signed)
 chan | number | Concentrator channel used for RX (unsigned integer)
 stat | number | CRC status: 1 = OK, -1 = fail, 0 = no CRC
 modu | string | Modulation identifier "LORA"
 datr | string | LoRa datarate identifier (eg. SF12BW500)
 codr | string | LoRa ECC coding rate identifier
 rssi | number | RSSI of the channel in dBm (signed integer, 1 dB precision)
 lsnr | number | Lora SNR ratio in dB (signed float, 0.1 dB precision)
 size | number | RF packet payload size in bytes (unsigned integer)
 data | string | Base64 encoded RF packet payload, padded

Example (white-spaces, indentation and newlines added for readability):

``` json
{"rxpk":[
	{
		"tmst":3512348611,
		"chan":2,
        "freq":2425.000000,
        "foff":729,
		"stat":1,
		"modu":"LORA",
		"datr":"SF12BW812",
		"codr":"4/8",
		"rssi":-35,
		"lsnr":5.1,
		"size":32,
		"data":"-DS4CGaDCdG+48eJNM3Vai-zDpsR71Pn9CPA9uCON84"
	},{
		"tmst":3316387610,
		"chan":0,
        "freq":2479.000000,
        "foff":-123,
		"stat":1,
		"modu":"LORA",
		"datr":"SF12BW812",
		"codr":"4/8",
		"rssi":-38,
		"lsnr":5.5,
		"size":32,
		"data":"ysgRl452xNLep9S1NTIg2lomKDxUgn3DJ7DE+b00Ass"
	}
]}
```

The root object can also contain an object named "stat" :

``` json
{
	"rxpk":[ {...}, ...],
	"stat":{...}
}
```

It is possible for a packet to contain no "rxpk" array but a "stat" object.

``` json
{
	"stat":{...}
}
```

That object contains the status of the gateway, with the following fields:

 Name |  Type  | Function
:----:|:------:|--------------------------------------------------------------
 time | string | UTC 'system' time of the gateway, ISO 8601 'expanded' format
 rxnb | number | Number of radio packets received (unsigned integer)
 rxok | number | Number of radio packets received with a valid PHY CRC
 rxfw | number | Number of radio packets forwarded (unsigned integer)
 ackr | number | Percentage of upstream datagrams that were acknowledged
 dwnb | number | Number of downlink datagrams received (unsigned integer)
 txnb | number | Number of packets emitted (unsigned integer)
 temp | number | Current temperature in degree celcius (float)

Example (white-spaces, indentation and newlines added for readability):

``` json
{"stat":{
    "time":"2014-01-12 08:59:28 GMT",
    "rxnb":2,
    "rxok":2,
    "rxfw":2,
    "ackr":100.0,
    "dwnb":2,
    "txnb":2,
    "temp":23.2
}}
```


## 5. Downstream protocol

### 5.1. Sequence diagram ###

	+---------+                                                    +---------+
	| Gateway |                                                    | Server  |
	+---------+                                                    +---------+
	     | -----------------------------------\                         |
	     |-| Every N seconds (keepalive time) |                         |
	     | ------------------------------------                         |
	     |                                                              |
	     | PULL_DATA (token Y, MAC@)                                    |
	     |------------------------------------------------------------->|
	     |                                                              |
	     |                                           PULL_ACK (token Y) |
	     |<-------------------------------------------------------------|
	     |                                                              |

	+---------+                                                    +---------+
	| Gateway |                                                    | Server  |
	+---------+                                                    +---------+
	     |      ------------------------------------------------------\ |
	     |      | Anytime after first PULL_DATA for each packet to TX |-|
	     |      ------------------------------------------------------- |
	     |                                                              |
	     |                            PULL_RESP (token Z, JSON payload) |
	     |<-------------------------------------------------------------|
	     |                                                              |
	     | TX_ACK (token Z, JSON payload)                               |
	     |------------------------------------------------------------->|

### 5.2. PULL_DATA packet ###

That packet type is used by the gateway to poll data from the server.

This data exchange is initialized by the gateway because it might be
impossible for the server to send packets to the gateway if the gateway is
behind a NAT.

When the gateway initialize the exchange, the network route towards the
server will open and will allow for packets to flow both directions.
The gateway must periodically send PULL_DATA packets to be sure the network
route stays open for the server to be used at any time.

 Bytes  | Function
:------:|---------------------------------------------------------------------
 0      | protocol version = 2
 1-2    | random token
 3      | PULL_DATA identifier 0x02
 4-11   | Gateway unique identifier (MAC address)

### 5.3. PULL_ACK packet ###

That packet type is used by the server to confirm that the network route is
open and that the server can send PULL_RESP packets at any time.

 Bytes  | Function
:------:|---------------------------------------------------------------------
 0      | protocol version = 2
 1-2    | same token as the PULL_DATA packet to acknowledge
 3      | PULL_ACK identifier 0x04

### 5.4. PULL_RESP packet ###

That packet type is used by the server to send RF packets and associated
metadata that will have to be emitted by the gateway.

 Bytes  | Function
:------:|---------------------------------------------------------------------
 0      | protocol version = 2
 1-2    | random token
 3      | PULL_RESP identifier 0x03
 4-end  | JSON object, starting with {, ending with }, see section 6

### 5.5. TX_ACK packet ###

That packet type is used by the gateway to send a feedback to the server
to inform if a downlink request has been accepted or rejected by the gateway.
The datagram may optionnaly contain a JSON string to give more details on
acknoledge. If no JSON is present (empty string), this means than no error
occured.

 Bytes  | Function
:------:|---------------------------------------------------------------------
 0      | protocol version = 2
 1-2    | same token as the PULL_RESP packet to acknowledge
 3      | TX_ACK identifier 0x05
 4-11   | Gateway unique identifier (MAC address)
 12-end | [optional] JSON object, starting with {, ending with }, see section 6


## 6. Downstream JSON data structure

The root object of PULL_RESP packet must contain an object named "txpk":

``` json
{
	"txpk": {...}
}
```

That object contain a RF packet to be emitted and associated metadata with the
following fields:

 Name |  Type  | Function
:----:|:------:|--------------------------------------------------------------
 imme | bool   | Send packet immediately (will ignore tmst & tmms)
 tmst | number | Send packet on a certain timestamp value (will ignore tmms)
 freq | number | TX central frequency in MHz (unsigned float, Hz precision)
 powe | number | TX output power in dBm (unsigned integer, dBm precision)
 modu | string | Modulation identifier "LORA"
 datr | string | LoRa datarate identifier (eg. SF12BW812)
 codr | string | LoRa ECC coding rate identifier
 ipol | bool   | Lora modulation polarization inversion
 prea | number | RF preamble size (unsigned integer)
 size | number | RF packet payload size in bytes (unsigned integer)
 data | string | Base64 encoded RF packet payload, padding optional
 ncrc | bool   | If true, disable the CRC of the physical layer (optional)

Most fields are optional.
If a field is omitted, default parameters will be used.

Examples (white-spaces, indentation and newlines added for readability):

``` json
{"txpk":{
	"imme":true,
	"freq":2422.000000,
	"powe":10,
	"modu":"LORA",
	"datr":"SF11BW812",
	"codr":"4/8",
	"ipol":false,
	"size":32,
	"data":"H3P3N2i9qc4yt7rK7ldqoeCVJGBybzPY5h1Dd7P7p8v"
}}
```

In case of error or warning, the root object of TX_ACK packet must contain an
object named "txpk_ack":

``` json
{
	"txpk_ack": {...}
}
```

That object contain status information concerning the associated PULL_RESP packet.

 Name |  Type  | Function
:----:|:------:|-----------------------------------------------------------------------------------------
error | string | Indicates the type of failure that occurred for downlink request (optional)
warn  | string | Indicates that downlink request has been accepted with limitation (optional)
value | string | When a warning is raised, it gives indications about the limitation (optional)
value | number | When a warning is raised, it gives indications about the limitation (optional)

The possible values of the "error" field are:

 Value             | Definition
:-----------------:|---------------------------------------------------------------------
 TOO_LATE          | Rejected because it was already too late to program this packet for downlink
 TOO_EARLY         | Rejected because downlink packet timestamp is too much in advance
 COLLISION_PACKET  | Rejected because there was already a packet programmed in requested timeframe
 COLLISION_BEACON  | Rejected because there was already a beacon planned in requested timeframe
 TX_FREQ           | Rejected because requested frequency is not supported by TX RF chain
 GPS_UNLOCKED      | Rejected because GPS is unlocked, so GPS timestamp cannot be used

The possible values of the "warn" field are:

 Value             | Definition
:-----------------:|---------------------------------------------------------------------
 TX_POWER          | The requested power is not supported by the gateway, the power actually used is given in the value field

Examples (white-spaces, indentation and newlines added for readability):

``` json
{"txpk_ack":{
	"error":"COLLISION_PACKET"
}}
```

``` json
{"txpk_ack":{
	"warn":"TX_POWER",
    "value":20
}}
```

## 7. Revisions

### v1.0 ###

* Initial version for 2.4Ghz
