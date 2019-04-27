# fhplayer Makefile
CC ?= cc
CFLAGS ?= -O2 -lm

CPPFLAGS += -pedantic -Wall -Wextra
CPPFLAGS += -I../farbherd/src

LDFLAGS += -lzstd

LIBS = sdl2
LDFLAGS += $(shell pkg-config --libs $(LIBS))
CPPFLAGS += $(shell pkg-config --cflags $(LIBS))

PREFIX ?= /usr/local
DESTDIR ?= /

BINS=fhplayer

all: $(BINS)

fhplayer: $(DEP) src/fhplayer.c src/fhstream.c src/fhstream.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o fhplayer src/fhplayer.c src/fhstream.c

.PHONY:
install: $(BINS)
	mkdir -p $(DESTDIR)/${PREFIX}/bin
	install $(BINS) $(DESTDIR)/$(PREFIX)/bin
uninstall:
	cd $(DESTDIR)/$(PREFIX)/bin; rm $(BINS)
clean:
	rm -f $(BINS)
