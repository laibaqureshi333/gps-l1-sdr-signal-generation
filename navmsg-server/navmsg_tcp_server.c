/*
 * navmsg_tcp_server.c
 *
 * Standalone GPS navigation-message generator + TCP streaming server.
 *
 * Derived from gps-sdr-sim's gpssim.c (MIT License, (c) 2015-2025 Takuji Ebinuma).
 * Only the navigation-message generation logic (RINEX nav parsing, subframe
 * encoding, GPS parity/checksum) is used here -- none of the RF/IQ sample
 * generation code. The functions eph2sbf(), generateNavMsg(),
 * computeChecksum(), countBits(), readRinexNavAll(), date2gps()/gps2date(),
 * subGpsTime()/incGpsTime() and replaceExpDesignator() are copied verbatim
 * from the original file.
 *
 * What this program does:
 *   1. Reads a RINEX navigation file for one satellite (PRN).
 *   2. Builds GPS subframes 1-5 for that satellite (eph2sbf).
 *   3. Runs generateNavMsg() every 30 seconds, exactly like the original
 *      simulator's main loop does, to produce the 30-bit data words
 *      (chan.dwrd[]) with correct TOW/week-number/parity.
 *   4. Opens a TCP server socket and streams those words to whatever
 *      client connects (e.g. a LabVIEW VI acting as a TCP client),
 *      at the real GPS nav-message bit rate: 50 bps, 30 bits/word =
 *      0.6 s/word, 10 words/subframe = 6 s/subframe, 5 subframes = 30 s/frame.
 *
 * Wire format (see README below for details):
 *   Each GPS word is sent as one 4-byte big-endian (network byte order)
 *   unsigned integer. Only the low 30 bits are meaningful (bits 31-30
 *   are always 0 in this encoding); LabVIEW should mask/keep the low 30 bits.
 *
 * Build:
 *   gcc -O2 -o navmsg_tcp_server navmsg_tcp_server.c -lm
 *
 * Run:
 *   ./navmsg_tcp_server <rinex_nav_file> <prn> <tcp_port>
 *   e.g. ./navmsg_tcp_server brdc0010.22n 12 9999
 *
 * Then point your LabVIEW TCP Client VI at 127.0.0.1:9999 (or the
 * machine's IP if LabVIEW runs elsewhere on the network).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

/* ---------------------------------------------------------------------
 * Cross-platform networking: Winsock2 on Windows, BSD sockets elsewhere.
 * ------------------------------------------------------------------- */
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#pragma comment(lib, "ws2_32.lib")   /* MSVC: auto-link Winsock */
	typedef SOCKET sock_t;
	typedef int socklen_t_compat;
	#define CLOSESOCK(s) closesocket(s)
	#define SOCK_INVALID(s) ((s) == INVALID_SOCKET)
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	typedef int sock_t;
	#define CLOSESOCK(s) close(s)
	#define SOCK_INVALID(s) ((s) < 0)
#endif

/* Portable millisecond sleep (used instead of POSIX nanosleep) */
static void sleep_ms(long ms)
{
#ifdef _WIN32
	Sleep((DWORD)ms);
#else
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
#endif
}

/* ---------------------------------------------------------------------
 * Constants and struct definitions (copied from gpssim.h, trimmed to
 * only what the nav-message code needs)
 * ------------------------------------------------------------------- */

#define TRUE  (1)
#define FALSE (0)

#define MAX_CHAR (100)
#define MAX_SAT  (32)
#define N_SBF      (5)   /* 5 subframes per frame */
#define N_DWRD_SBF (10)  /* 10 words per subframe */
#define N_DWRD ((N_SBF+1)*N_DWRD_SBF) /* 60: subframe-5 holding area + 5 subframes */

#define SECONDS_IN_WEEK   604800.0
#define SECONDS_IN_DAY     86400.0
#define SECONDS_IN_HOUR     3600.0
#define SECONDS_IN_MINUTE     60.0

#define POW2_M5  0.03125
#define POW2_M19 1.907348632812500e-6
#define POW2_M29 1.862645149230957e-9
#define POW2_M31 4.656612873077393e-10
#define POW2_M33 1.164153218269348e-10
#define POW2_M43 1.136868377216160e-13
#define POW2_M55 2.775557561562891e-17

#define POW2_M50 8.881784197001252e-016
#define POW2_M30 9.313225746154785e-010
#define POW2_M27 7.450580596923828e-009
#define POW2_M24 5.960464477539063e-008

#define PI 3.1415926535898

#define GM_EARTH 3.986005e14
#define OMEGA_EARTH 7.2921151467e-5

#define EPHEM_ARRAY_SIZE (15)

typedef struct
{
	int week;
	double sec;
} gpstime_t;

typedef struct
{
	int y,m,d,hh,mm;
	double sec;
} datetime_t;

typedef struct
{
	int vflg;
	datetime_t t;
	gpstime_t toc;
	gpstime_t toe;
	int iodc;
	int iode;
	double deltan;
	double cuc,cus,cic,cis,crc,crs;
	double ecc;
	double sqrta;
	double m0,omg0,inc0,aop;
	double omgdot,idot;
	double af0,af1,af2;
	double tgd;
	int svhlth;
	int codeL2;
	double n,sq1e2,A,omgkdot;
} ephem_t;

typedef struct
{
	int enable;
	int vflg;
	double alpha0,alpha1,alpha2,alpha3;
	double beta0,beta1,beta2,beta3;
	double A0,A1;
	int dtls,tot,wnt;
	int dtlsf,dn,wnlsf;
	int leapen;
} ionoutc_t;

/* Minimal channel struct: only what generateNavMsg()/eph2sbf() touch */
typedef struct
{
	int prn;
	gpstime_t g0;
	unsigned long sbf[5][N_DWRD_SBF];
	unsigned long dwrd[N_DWRD];
} channel_t;

/* ---------------------------------------------------------------------
 * Functions copied verbatim from gpssim.c
 * ------------------------------------------------------------------- */

#include "extracted_funcs.c"

/* ---------------------------------------------------------------------
 * TCP streaming server
 * ------------------------------------------------------------------- */

static sock_t open_listen_socket(int port)
{
	sock_t sock;
	int opt = 1;
	struct sockaddr_in addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (SOCK_INVALID(sock)) { perror("socket"); exit(1); }

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((u_short)port);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		exit(1);
	}

	if (listen(sock, 1) < 0)
	{
		perror("listen");
		exit(1);
	}

	return sock;
}

/* Send one GPS word (30 meaningful bits) as a 4-byte big-endian integer */
static int send_word(sock_t client_sock, unsigned long word)
{
	uint32_t netword = htonl((uint32_t)(word & 0x3FFFFFFFUL));
	int n = send(client_sock, (const char *)&netword, sizeof(netword), 0);
	return (n == (int)sizeof(netword)) ? 0 : -1;
}

int main(int argc, char *argv[])
{
	ephem_t eph[EPHEM_ARRAY_SIZE][MAX_SAT];
	ionoutc_t ionoutc = {0};
	channel_t chan = {0};
	int neph, ieph, sv, prn, port;
	gpstime_t grx;
	sock_t listen_sock, client_sock;
	struct sockaddr_in client_addr;
	socklen_t client_len;
	int word_index;

#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
	{
		fprintf(stderr, "ERROR: WSAStartup failed\n");
		return 1;
	}
#endif

	if (argc < 4)
	{
		fprintf(stderr, "Usage: %s <rinex_nav_file> <prn> <tcp_port>\n", argv[0]);
		fprintf(stderr, "  e.g.: %s brdc0010.22n 12 9999\n", argv[0]);
		return 1;
	}

	prn  = atoi(argv[2]);
	port = atoi(argv[3]);

	if (prn < 1 || prn > MAX_SAT)
	{
		fprintf(stderr, "ERROR: PRN must be between 1 and %d\n", MAX_SAT);
		return 1;
	}
	sv = prn - 1;

	/* --- 1. Read RINEX nav file --- */
	neph = readRinexNavAll(eph, &ionoutc, argv[1]);
	if (neph <= 0)
	{
		fprintf(stderr, "ERROR: No ephemeris data read from %s\n", argv[1]);
		return 1;
	}
	fprintf(stderr, "Read %d ephemeris sets from %s\n", neph, argv[1]);

	/* --- 2. Find the first valid ephemeris set for the requested PRN --- */
	ieph = -1;
	{
		int i;
		for (i = 0; i < neph; i++)
		{
			if (eph[i][sv].vflg == 1)
			{
				ieph = i;
				break;
			}
		}
	}
	if (ieph < 0)
	{
		fprintf(stderr, "ERROR: No valid ephemeris found for PRN %d in this file\n", prn);
		return 1;
	}

	/* Use this ephemeris set's time-of-ephemeris as our simulated "now" */
	grx = eph[ieph][sv].toe;

	fprintf(stderr, "Using PRN %d, ephemeris index %d, week %d, TOE sec %.1f\n",
		prn, ieph, grx.week, grx.sec);

	/* --- 3. Build subframes and initialize nav message --- */
	chan.prn = prn;
	eph2sbf(eph[ieph][sv], ionoutc, chan.sbf);
	generateNavMsg(grx, &chan, 1); /* init = 1: also fills subframe-5 holding words */

	/* --- 4. Open TCP server and wait for a client (e.g. LabVIEW) --- */
	listen_sock = open_listen_socket(port);
	fprintf(stderr, "Listening on TCP port %d ... waiting for a client to connect\n", port);

	client_len = sizeof(client_addr);
	client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
	if (SOCK_INVALID(client_sock))
	{
		perror("accept");
		return 1;
	}
	fprintf(stderr, "Client connected from %s:%d\n",
		inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

	/* Disable Nagle's algorithm so each word goes out immediately */
	{
		int one = 1;
		setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
	}

	/* --- 5. Stream words at real GPS nav-message timing ---
	 * chan.dwrd[0..9]  = subframe-5 holding words (NOT transmitted this frame,
	 *                    they become the reference for next frame's parity)
	 * chan.dwrd[10..59] = the 5 subframes (1,2,3,4,5) actually transmitted
	 *                    this 30-second frame -- 50 words total.
	 * Real GPS: 50 bits/sec, 30 bits/word => 0.6 s per word.
	 */
	fprintf(stderr, "Streaming navigation message words (0.6 s/word, 30 s/frame)...\n");
	fprintf(stderr, "Press Ctrl+C to stop.\n");

	while (1)
	{
		for (word_index = 10; word_index < N_DWRD; word_index++)
		{
			if (send_word(client_sock, chan.dwrd[word_index]) < 0)
			{
				fprintf(stderr, "Client disconnected. Exiting.\n");
				CLOSESOCK(client_sock);
				CLOSESOCK(listen_sock);
#ifdef _WIN32
				WSACleanup();
#endif
				return 0;
			}
			sleep_ms(600); /* 0.6 seconds */
		}

		/* Advance 30 seconds and regenerate the next frame's words,
		 * exactly as the original simulator's main loop does. */
		grx = incGpsTime(grx, 30.0);
		generateNavMsg(grx, &chan, 0);
	}

	CLOSESOCK(client_sock);
	CLOSESOCK(listen_sock);
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}
