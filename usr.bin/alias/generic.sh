#!/bin/sh
# $FreeBSD: src/usr.bin/alias/generic.sh,v 1.2 2005/10/24 22:32:19 cperciva Exp $
# $DragonFly: src/usr.bin/alias/generic.sh,v 1.3 2007/08/05 16:09:50 pavalos Exp $
# This file is in the public domain.
builtin ${0##*/} ${1+"$@"}
