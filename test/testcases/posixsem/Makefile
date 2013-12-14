# $FreeBSD: release/9.2.0/tools/regression/posixsem/Makefile 180059 2008-06-27 05:39:04Z jhb $

PROG=	posixsem
SRCS=	posixsem.c test.c semaphore.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
NO_MAN=
CFLAGS+= -g

WARNS?=	3

.include <bsd.prog.mk>
