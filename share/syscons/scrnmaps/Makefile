# $FreeBSD: src/share/syscons/scrnmaps/Makefile,v 1.14.2.3 2002/08/07 16:31:52 ru Exp $
# $DragonFly: src/share/syscons/scrnmaps/Makefile,v 1.3 2004/03/22 20:58:17 dillon Exp $

SCRMAPS = iso-8859-1_to_cp437.scm iso-8859-4_for_vga9.scm \
	  iso-8859-7_to_cp437.scm \
	  koi8-r2cp866.scm koi8-u2cp866u.scm \
	  us-ascii_to_cp437.scm

SCRMAPS_MK = ${SCRMAPS:R:S/$/.mk/g}
SCRDIR = ${SHAREDIR}/syscons/scrnmaps
CLEANFILES+= ${SCRMAPS_MK} ${SCRMAPS}

NOMAN = noman

all: ${SCRMAPS}

build-tools: ${SCRMAPS_MK}

install:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${SCRMAPS} ${DESTDIR}${SCRDIR}

${SCRMAPS}: ${.TARGET:R}.mk
	./${.TARGET:R}.mk ${.TARGET:R}.tmp
	uuencode ${.TARGET:R}.tmp ${.TARGET:R} > ${.TARGET}
	rm -f ${.TARGET:R}.tmp

${SCRMAPS_MK}: ${.TARGET:R} mkscrfil.c
	${NXCC} ${NXCFLAGS} -I${.CURDIR} -DFIL=\"${.TARGET:R}\" ${NXLDFLAGS} \
	    -o ${.TARGET} ${.CURDIR}/mkscrfil.c

.include <bsd.prog.mk>
