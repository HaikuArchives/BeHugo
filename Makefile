# Modified for BeOS by KT
# - You may have to change the locations of some files, like ncurses, etc.
# - Uncomment COMPILE_PPC to build PPC binaries using mwcc (or mwccppc)
#   (The default PPC tools, etc. are for x86-->PPC cross-compilation.)

# Hugo makefile for GCC and Unix by Bill Lash, modified by Kent Tessman
#
# (Anything really ugly in this makefile has come about since I
# got my hands on it.  --KT)
#
# Bill's (modified) notes:
#
# Make the executables hc, he, and hd by (editing this appropriately and)
# typing 'make'.
#
# Two of the features of this version of Hugo may cause some problems on
# some terminals.  These features are "special character support" and
# "color handling".
#
# "Special character support" allows some international characters to be
# printed, (e.g. grave-accented characters, characters with umlauts, ...).
# Unfortunately it is was beyond my ability to determine if a particular
# terminal supports these characters.  For example, the xterm that I use
# at work under SunOS 4.1.3 does not support these characters in the default
# "fixed" font family, but if started with xterm -fn courier, to use the
# "courier" font family, it does support these characters.  On my Linux box
# at home, the "fixed" font does support these characters.
#
# "Color handling" allows the game to control the color of text.
# This can only be enabled if using ncurses.  The color handling works pretty
# well on the Linux console, but in a color xterm and setting the TERM
# environment variable to xterm-color, annoying things can sometimes happen.
# I am not sure if the problem lies in the code here, the ncurses library,
# xterm, or the terminfo entry for xterm-color.  Using color rxvt for the
# terminal emulator produces the same results so I suspect the code here, ncurses
# or the terminfo entry.  If the TERM environment variable is xterm, no
# color will be used (although "bright" colors will appear bold).
#
# Note: After looking into this further, it seems that if one runs a
#	color xterm and sets the TERM environment variable to pcansi,
#	the color handling works very well (assuming that you use a
#	background color of black and a forground color of white or gray).
#	The command line to do this is:
#		color_xterm -bg black -fg white -tn pcansi
#	or
#		xterm-color -bg black -fg white -tn pcansi
#
# The following defines set configuration options:
#	DO_COLOR	turns on color handling (requires ncurses)
CFG_OPTIONS=-DDO_COLOR

# Set COMPILE_PPC if we're not compiling on x86, regardless
ifneq ($(BE_HOST_CPU), x86)
COMPILE_PPC=1
endif

# Uncomment this to do cross-compilation on an R5 installation with
# the x86-to-PPC tools installed:
#COMPILE_PPC=1

# Define your optimization flags.  Most compilers understand -O and -O2.
# Debugging:
#CFLAGS=-Wall -g 
# Standard optimizations:
# Pentium with gcc 2.7.0 or better:
#CFLAGS=-O2 -Wall -fomit-frame-pointer -malign-functions=2 -malign-loops=2 -malign-jumps=2
ifdef COMPILE_PPC
CFLAGS=-O4
else
CFLAGS=-O2 -Wall
endif

HC_CFLAGS:=$(CFLAGS) -DGCC_BEOS -DUSE_TEMPFILES -DBEOS
HE_CFLAGS:=$(CFLAGS) -DNCURSES -DGCC_BEOS -DNO_LATIN1_CHARSET
ifndef COMPILE_PPC
HE_CFLAGS:=$(HE_CFLAGS) -DBEOS
endif

ifeq ($(MAKECMDGOALS), hd)
HE_CFLAGS:=$(HE_CFLAGS) -DDEBUGGER -DFRONT_END
endif

# Need to change this to point to wherever you've got the ncurses library:
ifndef COMPILE_PPC
NCURSES_PATH=/boot/home/work/dev/ncurses
else
NCURSES_PATH=/boot/home/work/dev/ncurses/ppc
endif

# Tools and libraries:
ifdef COMPILE_PPC
PPC_TOOLS_DIR=/boot/develop/tools/develop_ppc
CC=$(PPC_TOOLS_DIR)/bin/mwccppc
LD=$(PPC_TOOLS_DIR)/bin/mwldppc
HC_LIBS=-nostdlib -l $(PPC_TOOLS_DIR)/lib/ppc start_dyn.o init_term_dyn.o glue-noinit.a libroot.so
HE_LIBS=$(HC_LIBS) -l $(NCURSES_PATH)/lib libncurses.a
else
CC=gcc
LD=gcc
HC_LIBS=
HE_LIBS=-L$(NCURSES_PATH)/lib -lncurses
endif

# If you need a special include path to get the right curses header, specify
# it.  Note that -fwritable-strings is required for gcc and this package.
ifdef COMPILE_PPC
HC_CC=/boot/develop/tools/develop_ppc/bin/mwccppc $(HC_CFLAGS) -i source
HE_CC=/boot/develop/tools/develop_ppc/bin/mwccppc $(HE_CFLAGS) -i source -i- -i $(NCURSES_PATH)/include
else
HC_CC=gcc -Isource $(HC_CFLAGS)
HE_CC=gcc -I$(NCURSES_PATH)/include -Isource $(CFG_OPTIONS) $(HE_CFLAGS)
endif

##############################################################################
# Shouldn't need to change anything below here.
ifdef COMPILE_PPC
HC_TARGET=hc_ppc
HE_TARGET=he_ppc
HD_TARGET=hd_ppc
else
HC_TARGET=hc
HE_TARGET=he
HD_TARGET=hd
endif
HC_H=source/hcheader.h source/htokens.h
HE_H=source/heheader.h
HD_H=$(HE_H) source/hdheader.h source/hdinter.h source/htokens.h
HC_OBJS = hc.o hcbuild.o hccode.o hcdef.o hcfile.o hclink.o \
hcmisc.o hccomp.o hcpass.o hcres.o stringfn.o hcgcc.o
HE_OBJS = he.o heexpr.o hemisc.o heobject.o heparse.o herun.o \
heres.o heset.o stringfn.o hegcc.o
HD_OBJS = hd.o hddecode.o hdmisc.o hdtools.o hdupdate.o \
hdval.o hdwindow.o hdgcc.o

#all: hc he iotest
all:
	$(MAKE) hc
	$(RM) he*.o
	$(MAKE) he
	$(RM) he*.o
	$(MAKE) hd

hc:	$(HC_OBJS)
#	gcc -g -o hc $(HC_OBJS) $(HC_LIBS)
	$(LD) -o $(HC_TARGET) $(HC_OBJS) $(HC_LIBS)

he:	$(HE_OBJS)
#	gcc -g -o he $(HE_OBJS) $(HE_LIBS)
	$(LD) -o $(HE_TARGET) $(HE_OBJS) $(HE_LIBS)

hd:	$(HE_OBJS) $(HD_OBJS)
#	gcc -g -o hd $(HD_OBJS) $(HE_LIBS)
	$(LD) -o $(HD_TARGET) $(HE_OBJS) $(HD_OBJS) $(HE_LIBS)

iotest:	source/iotest.c gcc/hegcc.c $(HE_H)
	$(CC) -o iotest source/iotest.c hegcc.o stringfn.o $(HE_LIBS)

clean:
	rm -f $(HC_OBJS) $(HD_OBJS) $(HE_OBJS)

# Portable sources:

hc.o: source/hc.c $(HC_H)
	$(HC_CC) -c source/hc.c

hcbuild.o: source/hcbuild.c $(HC_H)
	$(HC_CC) -c source/hcbuild.c

hccode.o: source/hccode.c $(HC_H)
	$(HC_CC) -c source/hccode.c

hccomp.o: source/hccomp.c $(HC_H)
	$(HC_CC) -c source/hccomp.c

hcdef.o: source/hcdef.c $(HC_H)
	$(HC_CC) -c source/hcdef.c

hcfile.o: source/hcfile.c $(HC_H)
	$(HC_CC) -c source/hcfile.c

hclink.o: source/hclink.c $(HC_H)
	$(HC_CC) -c source/hclink.c

hcmisc.o: source/hcmisc.c $(HC_H)
	$(HC_CC) -c source/hcmisc.c

hcpass.o: source/hcpass.c $(HC_H)
	$(HC_CC) -c source/hcpass.c

hcres.o: source/hcres.c $(HC_H)
	$(HC_CC) -c source/hcres.c

hd.o: source/hd.c $(HD_H) $(HE_H)
	$(HE_CC) -c source/hd.c

hddecode.o: source/hddecode.c $(HD_H) $(HE_H)
	$(HE_CC) -c source/hddecode.c

hdmisc.o: source/hdmisc.c $(HD_H) $(HE_H)
	$(HE_CC) -c source/hdmisc.c

hdtools.o: source/hdtools.c $(HD_H) $(HE_H)
	$(HE_CC) -c source/hdtools.c

hdupdate.o: source/hdupdate.c $(HD_H) $(HE_H)
	$(HE_CC) -c source/hdupdate.c

hdval.o: source/hdval.c $(HD_H) $(HE_H)
	$(HE_CC) -c source/hdval.c

hdwindow.o: source/hdwindow.c $(HD_H) $(HE_H)
	$(HE_CC) -c source/hdwindow.c

he.o: source/he.c $(HE_H)
	$(HE_CC) -c source/he.c

heexpr.o: source/heexpr.c $(HE_H)
	$(HE_CC) -c source/heexpr.c

hemisc.o: source/hemisc.c $(HE_H)
	$(HE_CC) -c source/hemisc.c

heobject.o: source/heobject.c $(HE_H)
	$(HE_CC) -c source/heobject.c

heparse.o: source/heparse.c $(HE_H)
	$(HE_CC) -c source/heparse.c

heres.o: source/heres.c $(HE_H)
	$(HE_CC) -c source/heres.c

herun.o: source/herun.c $(HE_H)
	$(HE_CC) -c source/herun.c

heset.o: source/heset.c $(HE_H)
	$(HE_CC) -c source/heset.c

stringfn.o: source/stringfn.c
	$(HE_CC) -c source/stringfn.c


# Non-portable sources:

hcgcc.o: gcc/hcgcc.c $(HC_H)
	$(HC_CC) -c gcc/hcgcc.c

hegcc.o: gcc/hegcc.c $(HE_H)
	$(HE_CC) -c gcc/hegcc.c

hdgcc.o: gcc/hdgcc.c $(HD_H)
	$(HE_CC) -c gcc/hdgcc.c
