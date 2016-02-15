NAME = dwmstatus

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
MPDLIB   =  -lmpdclient
ALSALIB  =  -lasound
INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -lm -L${X11LIB} -lX11 ${MPDLIB} $(ALSALIB)

# flags
MPDFLAG  =  -DMPD
CPPFLAGS = ${MPDFLAG} -O2 -march=haswell -mtune=haswell -fomit-frame-pointer -pipe
CFLAGS = -g -std=c99 -pedantic -Wall ${INCS} ${CPPFLAGS}
LDFLAGS = -g ${LIBS}

# compiler and linker
CC = cc

