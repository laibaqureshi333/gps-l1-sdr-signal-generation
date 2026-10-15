# GPS Navigation Message TCP Server (C)

Generates the GPS L1 **navigation message** for one satellite from a RINEX v2
navigation file and streams it over TCP, so that LabVIEW
(`../labview/navtcpconversion.vi`) can receive it as a TCP client.

Only the navigation-message logic (RINEX parsing, subframe encoding, GPS
parity) is taken from [gps-sdr-sim](https://github.com/osqzss/gps-sdr-sim)
(`gpssim.c`, MIT License). None of its RF/IQ signal generation is used. See
[`../THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

## Files

| File | Purpose |
|---|---|
| `navmsg_tcp_server.c` | Main program: reads RINEX, builds subframes for one PRN, streams 30-bit words over TCP |
| `extracted_funcs.c` | Navigation-message functions copied from `gpssim.c` (`eph2sbf`, `generateNavMsg`, `computeChecksum`, `readRinexNavAll`, time conversions, ...), included by the main file |
| `data/brdc1000.26n` | Sample IGS broadcast ephemeris (RINEX v2), day 100 of 2026 |

## Build

The code detects the platform and uses Winsock2 on Windows or BSD sockets on Linux/macOS.

```bash
# Linux / macOS / WSL
gcc -O2 -o navmsg_tcp_server navmsg_tcp_server.c -lm
```
```powershell
# Windows (MinGW)
gcc -O2 -o navmsg_tcp_server.exe navmsg_tcp_server.c -lws2_32

# Windows (Visual Studio developer prompt)
cl navmsg_tcp_server.c ws2_32.lib
```

## Run

```bash
./navmsg_tcp_server data/brdc1000.26n 1 9999
#                   <rinex_nav_file>  <prn> <tcp_port>
```

The server prints its status, then waits for **one** TCP client. Example start-up output:

```
Read 13 ephemeris sets from brdc1000.26n
Using PRN 1, ephemeris index 0, week 2413, TOE sec 432000.0
Listening on TCP port 9999 ... waiting for a client to connect
```

## Wire format

Each navigation word is sent as one **4-byte, big-endian (network order) unsigned 32-bit integer**.
Only the low 30 bits are meaningful (24 data bits + 6 parity bits, ICD-GPS-200 word format).

Timing follows the real GPS signal:

- 50 bit/s navigation data, 30 bits/word, so one word every **0.6 s**
- 10 words per subframe, so **6 s per subframe**
- 5 subframes per frame, so **30 s per frame**, then it repeats with an updated TOW

## LabVIEW client

`navtcpconversion.vi` opens a TCP connection to the server, reads 4 bytes per word,
converts them to a U32 (LabVIEW unflatten/typecast is big-endian by default, so no byte
swapping is needed), masks the low 30 bits, and converts the words to individual navigation bits.

## Limitations

- One PRN per server instance. Run several instances on different ports for several satellites.
- Streams raw words rather than decoded ephemeris fields.
