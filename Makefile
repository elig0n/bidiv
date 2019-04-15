PREFIX=/usr/local
BIN_DIR=$(PREFIX)/bin
MAN_PATH=$(PREFIX)/man
CC_OPT_FLAGS=-O2 -Wall


CFLAGS= $(CC_OPT_FLAGS) $(DEFS) `pkg-config --cflags fribidi`
LDFLAGS=`pkg-config --libs fribidi`

all: bidiv

bidiv: bidiv.o
	$(CC) -o bidiv bidiv.o $(LDFLAGS)

clean:
	rm -f bidiv.o *~

clobber: clean
	rm -f bidiv

.c.o:
	$(CC) -c $(CFLAGS) $<

install: all
	strip bidiv
	cp bidiv $(BIN_DIR)
	chmod 755 $(BIN_DIR)/bidiv
	cp bidiv.1 $(MAN_PATH)/man1/bidiv.1
	chmod 644 $(MAN_PATH)/man1/bidiv.1

uninstall:
	rm -f $(BIN_DIR)/bidiv $(MAN_PATH)/man1/bidiv.1
