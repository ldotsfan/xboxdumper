OBJS=main.o util.o fatx.o dir.o partition.o
MKFS=mkfs.o util.o fatx.o dir.o partition.o
CFLAGS=-O2 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D__USE_LARGEFILE64 -Wall

all: xboxdumper mkfs.fatx

xboxdumper: ${OBJS}
	gcc -o $@ -static ${OBJS} ${CFLAGS}
	strip $@

mkfs.fatx: ${MKFS}
	gcc -o $@ -static ${MKFS} ${CFLAGS}
	strip $@

clean:
	rm -f $(OBJS) $(MKFS) xboxdumper mkfs.fatx *# *~

.PHONY: clean

%.o	: %.c 
	gcc ${CFLAGS} -o $@ -c $<

install:
	cp mkfs.fatx xboxdumper $(DESTDIR)/sbin/
	chmod 0700 $(DESTDIR)/sbin/mkfs.fatx
	chmod 0700 $(DESTDIR)/sbin/xboxdumper
