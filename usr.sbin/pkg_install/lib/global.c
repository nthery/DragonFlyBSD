/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 *
 * 18 July 1993
 *
 * Semi-convenient place to stick some needed globals.
 *
 * $FreeBSD: src/usr.sbin/pkg_install/lib/global.c,v 1.9 2002/04/01 09:39:07 obrien Exp $
 * $DragonFly: src/usr.sbin/pkg_install/lib/Attic/global.c,v 1.3 2004/07/30 04:46:13 dillon Exp $
 */

#include "lib.h"

/* These are global for all utils */
Boolean	Verbose		= FALSE;
Boolean	Fake		= FALSE;
Boolean	Force		= FALSE;
int AutoAnswer		= FALSE;
