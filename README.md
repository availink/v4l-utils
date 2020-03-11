## avl-scan
### What?
This new utility implements Availink's fast blind scan algorithm in userspace.
*Blind* means that, given a frequency range, it will search the entire spectrum.
Just tell avl-scan what type of LNB you have and it does the rest, outputting
a list of transport streams in the DVBv5 file format (including those transported
as multipel ISI's and/or inside T2MI).
This channel file can be fed into e.g. dvbv5-scan for program scanning.
It's *fast* because it searches intelligently - it doesn't just try every
frequency/symbol rate combination, or a rasterized reduction of that. A full
scan of the L-band spectrum takes about a minute, depending on how many
channels there are.
### Why?
There aren't any open-source truly *blind* scanning applications.  Especially none
that are written to use the DVBv5 API.  Why rely on potentially out-of-date "scan files"?
Why rely on NIT to tell you what other transponders are out there?
Want to know what's coming off the bird your dish is pointed to? Just run avl-scan.
Why didn't we integrate program scan into avl-scan? Because there are already good tools
for doing that.
### How?
Here's an example where we scan both bands of a universal LNB and find two TS's from
one transponder at 11.3GHz, DVB-S2 @ 20Msps, 2 ISI's.
```
$ avl-scan -l UNIVERSAL -o mychans.conf
Using LNB 'UNIVERSAL'. Frequency bands:
	0: 10800 MHz to 11800 MHz, LO 9750 MHz
	1: 11600 MHz to 12700 MHz, LO 10600 MHz

Frontend: Availink avl62x1
	Fmin  950 MHz
	Fmax  2150 MHz
	SRmin 1000 Ksps
	SRmax 55000 Ksps


Scanning frequency band 0: 10800 MHz to 11800 MHz, LO 9750 MHz
IF from 1050 MHz to 2050 MHz


Ftune 1050.000 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1107.577 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1165.154 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1222.731 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1280.308 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1337.885 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1395.463 MHz
No streams
Step tuner by 16.008 MHz


Ftune 1411.470 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1447.456 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1483.442 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1541.019 MHz
IF 1550.033081 MHz
Freq 11300.033203 MHz
Symrate 19.982 Msps
ISI 1
STD S2


Ftune 1541.019 MHz
IF 1550.033081 MHz
Freq 11300.033203 MHz
Symrate 19.982 Msps
ISI 2
STD S2
Step tuner by 56.484 MHz


Ftune 1597.503 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1655.080 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1712.657 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1770.234 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1827.811 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1863.797 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1899.783 MHz
No streams
Step tuner by 62.999 MHz


Ftune 1962.781 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1998.767 MHz
No streams
Step tuner by 35.986 MHz


Ftune 2034.753 MHz
No streams
Step tuner by 57.577 MHz


Ftune 2050.000 MHz
No streams


Scanning frequency band 1: 11600 MHz to 12700 MHz, LO 10600 MHz
IF from 1000 MHz to 2100 MHz


Ftune 1000.000 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1057.577 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1115.154 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1172.731 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1208.717 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1266.294 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1323.871 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1381.448 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1439.025 MHz
No streams
Step tuner by 23.869 MHz


Ftune 1462.894 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1520.471 MHz
No streams
Step tuner by 47.306 MHz


Ftune 1567.777 MHz
No streams
Step tuner by 51.971 MHz


Ftune 1619.748 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1677.325 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1734.902 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1792.479 MHz
No streams
Step tuner by 57.577 MHz


Ftune 1850.056 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1886.042 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1922.028 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1958.014 MHz
No streams
Step tuner by 35.986 MHz


Ftune 1993.999 MHz
No streams
Step tuner by 35.986 MHz


Ftune 2029.985 MHz
No streams
Step tuner by 57.577 MHz


Ftune 2087.562 MHz
No streams
Step tuner by 62.734 MHz


Ftune 2100.000 MHz
No streams

Found a total of 2 channels
Exiting...

$ cat mychans.conf
[CHANNEL]
	FREQUENCY = 11300033
	SYMBOL_RATE = 19982456
	DELIVERY_SYSTEM = DVBS2
	PILOT = ON
	STREAM_ID = 1
	POLARIZATION = OFF
	INVERSION = AUTO
	INNER_FEC = AUTO
	MODULATION = QAM/AUTO
	ROLLOFF = AUTO

[CHANNEL]
	FREQUENCY = 11300033
	SYMBOL_RATE = 19982456
	DELIVERY_SYSTEM = DVBS2
	PILOT = ON
	STREAM_ID = 2
	POLARIZATION = OFF
	INVERSION = AUTO
	INNER_FEC = AUTO
	MODULATION = QAM/AUTO
	ROLLOFF = AUTO
$
```
Now that you have a channels file, you can feed it into dvbv5-scan to find out what programs are in there.
Here there are four - one in one TS (STREAM_ID 1) and three in the other TS (STREAM_ID 2).
```
$ dvbv5-scan -l UNIVERSAL -a 0 -f 0 -d 1 mychans.conf 
Using LNBf UNIVERSAL
	Universal, Europe
	Freqs     : 10800 to 11800 MHz, LO: 9750 MHz
	Freqs     : 11600 to 12700 MHz, LO: 10600 MHz
Scanning frequency #1 11300033
       (0x00)
Lock   (0x10) Signal= -0.04dBm C/N= 22.93dB
	  Layer A: Signal= 77.00% C/N= 83.64%
Service Diver, provider Rohde & Schwarz: digital television
New transponder/channel found: #3: 11719500
Scanning frequency #2 11300033
       (0x00) Signal= -0.04dBm
Lock   (0x10) Signal= -0.04dBm C/N= 22.62dB
	  Layer A: Signal= 77.00% C/N= 82.64%
Service BBC HD, provider BBC: reserved
Service ITV1 HD, provider ITV: reserved
Service Channel 4 HD, provider CHANNEL FOUR: reserved
Scanning frequency #3 11719500
       (0x00) Signal= -0.04dBm
$ cat dvb_channel.conf 
[Diver]
	SERVICE_ID = 1
	NETWORK_ID = 1999
	TRANSPORT_ID = 2241
	VIDEO_PID = 256
	AUDIO_PID = 272
	LNB = UNIVERSAL
	FREQUENCY = 11300033
	INVERSION = AUTO
	SYMBOL_RATE = 19982456
	INNER_FEC = AUTO
	MODULATION = QAM/AUTO
	PILOT = ON
	ROLLOFF = AUTO
	POLARIZATION = OFF
	STREAM_ID = 1
	DELIVERY_SYSTEM = DVBS2

[BBC HD]
	SERVICE_ID = 17472
	NETWORK_ID = 9018
	TRANSPORT_ID = 4100
	VIDEO_PID = 101
	AUDIO_PID = 102 106
	PID_0b = 130 111 110
	PID_06 = 105
	PID_05 = 150
	LNB = UNIVERSAL
	FREQUENCY = 11300033
	INVERSION = AUTO
	SYMBOL_RATE = 19982456
	INNER_FEC = AUTO
	MODULATION = QAM/AUTO
	PILOT = ON
	ROLLOFF = AUTO
	POLARIZATION = OFF
	STREAM_ID = 2
	DELIVERY_SYSTEM = DVBS2

[ITV1 HD]
	SERVICE_ID = 17604
	NETWORK_ID = 9018
	TRANSPORT_ID = 4100
	VIDEO_PID = 201
	AUDIO_PID = 202 206
	PID_06 = 205
	LNB = UNIVERSAL
	FREQUENCY = 11300033
	INVERSION = AUTO
	SYMBOL_RATE = 19982456
	INNER_FEC = AUTO
	MODULATION = QAM/AUTO
	PILOT = ON
	ROLLOFF = AUTO
	POLARIZATION = OFF
	STREAM_ID = 2
	DELIVERY_SYSTEM = DVBS2

[Channel 4 HD]
	SERVICE_ID = 17664
	NETWORK_ID = 9018
	TRANSPORT_ID = 4100
	VIDEO_PID = 301
	AUDIO_PID = 302 306
	PID_06 = 305
	LNB = UNIVERSAL
	FREQUENCY = 11300033
	INVERSION = AUTO
	SYMBOL_RATE = 19982456
	INNER_FEC = AUTO
	MODULATION = QAM/AUTO
	PILOT = ON
	ROLLOFF = AUTO
	POLARIZATION = OFF
	STREAM_ID = 2
	DELIVERY_SYSTEM = DVBS2
```
Or, you can copy your mychans.conf file into tvheadend's dvb-scan/dvb-s/ directory,
give it a good name like "PrettyBird-75W", and then select it as a "Pre-defined mux"
when adding a DVB-S network.
### Where?
You'll find the source in utils/dvb/avl-scan.c
### Fine Print
This utility relies on magic implemented in our Availink demodulator firmwares and Linux drivers.
At this time, only the avl62x1 driver implements this magic, but support for the avl68x2 is in the works.
