#
# Copyright (c) 2024 Stefanos Stefanidis.
# All rights reserved.
#

#
# Machine-independent kernel initialization source files.
#
SRCS = \
	init_main.c \
	init_standsh.c \
	init_sysent.c

OBJS = \
	init_main.o \
	init_standsh.o \
	init_sysent.o

BUNDLE = init.a

all: makeobjs makear

#
# Create the object files to bundle together later.
#
makeobjs:
	$(CC) $(CFLAGS) -c $(SRCS)

#
# Create an ar archive that contains the built object files.
# This ar archive is linked, along with its supporting ar archives and
# machine-dependent object files, to form the kernel image.
#
makear:
	$(AR) crv $(BUNDLE) $(OBJS)

clean:
	rm $(BUNDLE) $(OBJS)
