/*	$OpenBSD: src/usr.sbin/rdate/ntpleaps.c,v 1.7 2004/05/05 20:29:54 jakob Exp $	*/
/*	$DragonFly: src/usr.sbin/rdate/ntpleaps.c,v 1.1 2004/12/01 15:04:43 joerg Exp $ */

/*
 * Copyright (c) 2002 Thorsten Glaser. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Leap second support for NTP clients (generic) */

/*
 * I could include tzfile.h, but this would make the code unportable
 * at no real benefit. Read tzfile.h for why.
 */

#include <sys/types.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpleaps.h"

static uint32_t	read_be_dword(uint8_t *ptr);

static uint64_t *leapsecs = NULL;
static unsigned int leapsecs_num = 0;
static int flaginit = -1;
static int flagwarn = 0;

int
ntpleaps_init(void)
{
	if (!flaginit)
		return(0);

	if (!ntpleaps_read()) {
		flaginit = 0;
		return(0);
	}

	/* This does not really hurt, but users will complain about
	 * off-by-22-seconds (at time of coding) errors if we don't warn.
	 */
	if (!flagwarn) {
		fputs("Warning: error reading tzfile. You will NOT be\n"
		    "able to get legal time or posix compliance!\n", stderr);
		flagwarn = 1;	/* put it only once */
	}

	return(-1);
}

int
ntpleaps_sub(uint64_t *t)
{
	unsigned int i = 0;
	u_int64_t u;
	int r = 1;

	if ((flaginit ? ntpleaps_init() : 0) == -1)
		return(-1);

	u = *t;

	while (i < leapsecs_num) {
		if (u < leapsecs[i])
			break;
		if (u == leapsecs[i++])
			goto do_sub;
	}
	--r;

do_sub:
	*t = u - i;
	return(r);
}

uint32_t
read_be_dword(uint8_t *ptr)
{
	uint32_t res;

	memcpy(&res, ptr, 4);
	return(ntohl(res));
}

int
ntpleaps_read(void)
{
	int fd;
	unsigned int r;
	uint8_t buf[32];
	uint32_t m1, m2, m3;
	uint64_t s;
	uint64_t *l;

	fd = open("/usr/share/zoneinfo/Etc/UTC", O_RDONLY | O_NDELAY);
	if (fd == -1)
		return (-1);

	/* Check signature */
	read(fd, buf, 4);
	buf[4] = 0;
	if (strcmp((const char *)buf, "TZif")) {
		close(fd);
		return (-1);
	}

	/* Pre-initalize buf[24..27] so we need not check read(2) result */
	buf[24] = 0;
	buf[25] = 0;
	buf[26] = 0;
	buf[27] = 0;

	/* Skip uninteresting parts of header */
	read(fd, buf, 28);

	/* Read number of leap second entries */
	r = read_be_dword(&buf[24]);
	/* Check for plausibility - arbitrary values */
	if ((r < 20) || (r > 60000)) {
		close(fd);
		return (-1);
	}
	if ((l = (uint64_t *)malloc(r << 3)) == NULL) {
		close(fd);
		return (-1);
	}

	/* Skip further uninteresting stuff */
	read(fd, buf, 12);
	m1 = read_be_dword(buf);
	m2 = read_be_dword(&buf[4]);
	m3 = read_be_dword(&buf[8]);
	m3 += (m1 << 2) + m1 + (m2 << 2) + (m2 << 1);
	lseek(fd, (off_t)m3, SEEK_CUR);

	/* Now go parse the tzfile leap second info */
	for (m1 = 0; m1 < r; m1++) {
		if (read(fd, buf, 8) != 8) {
			free(l);
			close(fd);
			return(-1);
		}
		s = SEC_TO_TAI64(read_be_dword(buf));
		/*
		 * Assume just _one_ leap second on each entry, and compensate
		 * the lacking error checking by validating the first entry
		 * against the known value
		 */
		if (!m1 && s != 0x4000000004B2580AULL)
			return(-1);
		l[m1] = s;
	}

	/* Clean up and activate the table */
	close(fd);
	if (leapsecs != NULL)
		free(leapsecs);
	leapsecs = l;
	leapsecs_num = r;
	return(0);
}
