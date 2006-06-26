# $FreeBSD: src/share/skel/dot.profile,v 1.19.2.2 2002/07/13 16:29:10 mp Exp $
# $DragonFly: src/share/skel/dot.profile,v 1.4 2006/06/26 00:13:37 geekgod Exp $
#
# .profile - Bourne Shell startup script for login shells
#
# see also sh(1), environ(7).
#

# remove /usr/games and /usr/X11R6/bin if you want
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/games:/usr/pkg/sbin:/usr/pkg/bin:/usr/pkg/xorg/bin:/usr/local/sbin:/usr/local/bin:$HOME/bin; export PATH

# Setting TERM is normally done through /etc/ttys.  Do only override
# if you're sure that you'll never log in via telnet or xterm or a
# serial line.
# Use cons25l1 for iso-* fonts
# TERM=cons25; 	export TERM

BLOCKSIZE=K;	export BLOCKSIZE
EDITOR=vi;   	export EDITOR
PAGER=more;  	export PAGER

# set ENV to a file invoked each time sh is started for interactive use.
ENV=$HOME/.shrc; export ENV

[ -x /usr/games/fortune ] && /usr/games/fortune dragonfly-tips
