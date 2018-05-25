/*

Copyright 2018 Intel Corporation

This software and the related documents are Intel copyrighted materials,
and your use of them is governed by the express license under which they
were provided to you (License). Unless the License provides otherwise,
you may not use, modify, copy, publish, distribute, disclose or transmit
this software or the related documents without Intel's prior written
permission.

This software and the related documents are provided as is, with no
express or implied warranties, other than those that are expressly stated
in the License.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sgx_urts.h>
#ifdef _WIN32
# include <Windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")
#else
# include <unistd.h>
#endif
#include "hexutil.h"
#include "msgio.h"
#include "common.h"

#define BUFFER_SZ	1024*1024

static char *buffer = NULL;
static uint32_t buffer_size = BUFFER_SZ;

/* With no arguments, we read/write to stdin/stdout using stdio */

MsgIO::MsgIO()
{
	bool use_stdio = true;
}

/* Connect to a remote server and port, and use socket IO */

MsgIO::MsgIO(const char *server, uint16_t port)
{
#ifdef _WIN32
	WSADATA wsa;
#endif
	int rv;
	struct addrinfo *ainfo, hints;

#ifdef _WIN32
	rv = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (rv != 0) {
		eprintf("WSAStartup: %d\n", rv);
		throw std::runtime_error("WSAStartup failed");
	}
#endif
	/* Listening socket */

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;

	rv= getaddrinfo(NULL, server, &hints, &ainfo);
	if (rv != 0) {		
		eprintf("getaddrinfo: %s\n", gai_strerror(rv));
		throw std::runtime_error(gai_strerror(rv));
	}
	
	/* Sending socket */
#ifdef _WIN32

#endif
}

MsgIO::~MsgIO()
{
}

int MsgIO::read(void **dest, size_t *sz)
{
	if (use_stdio) return read_msg(dest, sz);
}

void MsgIO::send(void *src, size_t sz)
{
	if (use_stdio) {
		send_msg(src, sz);
		return;
	}
}

void MsgIO::send_partial(void *src, size_t sz)
{
	if (use_stdio) {
		send_msg_partial(src, sz);
		return;
	}
}

/*
* Read a msg from stdin. There's a fixed portion and a variable-length
* payload.
*
* Our destination buffer is the entire message plus the payload,
* returned to the caller. This lets them cast it as an sgx_ra_msgN_t
* structure, and take advantage of the flexible array member at the
* end (if it has one, as sgx_ra_msg2_t and sgx_ra_msg3_t do).
*
* All messages are base16 encoded (ASCII hex strings) for simplicity
* and readability. We decode the message into the destination.
*
* We do not allow any whitespace in the incoming message, save for the
* terminating newline.
*
*/


int read_msg(void **dest, size_t *sz)
{
	size_t bread;
	int repeat = 1;

	if (buffer == NULL) {
		buffer = (char *)malloc(buffer_size);
		if (buffer == NULL) {
			perror("malloc");
			return -1;
		}
	}

	bread = 0;
	while (repeat) {
		if (fgets(&buffer[bread], (int)(buffer_size - bread), stdin) == NULL) {
			if (ferror(stdin)) {
				perror("fgets");
				return -1;
			}
			else {
				fprintf(stderr, "EOF received\n");
				return 0;
			}
		}
		/* If the last char is not a newline, we have more reading to do */

		bread = strlen(buffer);
		if (bread == 0) {
			fprintf(stderr, "EOF received\n");
			return 0;
		}

		if (buffer[bread - 1] == '\n') {
			repeat = 0;
			--bread;	/* Discard the newline */
		}
		else {
			buffer_size += BUFFER_SZ;
			buffer = (char *) realloc(buffer, buffer_size);
			if (buffer == NULL) return -1;
		}
	}

	/* Make sure we didn't get \r\n */
	if (bread && buffer[bread - 1] == '\r') --bread;

	if (bread % 2) {
		fprintf(stderr, "read odd byte count %zu\n", bread);
		return 0;	/* base16 encoding = even number of bytes */
	}

	*dest = malloc(bread / 2);
	if (*dest == NULL) return -1;

	if (debug) {
		edividerWithText("read buffer");
		eputs(buffer);
		edivider();
	}

	from_hexstring((unsigned char *) *dest, buffer, bread / 2);

	if (sz != NULL) *sz = bread;

	return 1;
}

/* Send a partial message (no newline) */

void send_msg_partial(void *src, size_t sz) {
	if (sz) print_hexstring(stdout, src, sz);
#ifndef _WIN32
	/*
	* If were aren't writing to a tty, also print the message to stderr
	* so you can still see the output.
	*/
	if (!isatty(fileno(stdout))) print_hexstring(stderr, src, sz);
#endif
}

void send_msg(void *src, size_t sz)
{
	if (sz) print_hexstring(stdout, src, sz);
	printf("\n");
#ifndef _WIN32
	/* As above */
	if (!isatty(fileno(stdout))) {
		print_hexstring(stderr, src, sz);
		fprintf(stderr, "\n");
	}
#endif

	/*
	* Since we have both stdout and stderr, flush stdout to keep the
	* the output stream synchronized.
	*/

	fflush(stdout);
}


/* Send a partial message (no newline) */

void fsend_msg_partial(FILE *fp, void *src, size_t sz) {
	if (sz) print_hexstring(fp, src, sz);
}

void fsend_msg(FILE *fp, void *src, size_t sz)
{
	if (sz) print_hexstring(fp, src, sz);
	fprintf(fp, "\n");
	fflush(fp);
}

