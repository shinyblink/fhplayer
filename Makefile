# fhplayer Makefile
CC ?= cc
CFLAGS ?= -O2 -lm

CPPFLAGS += -pedantic -Wall -Wextra
CPPFLAGS += -I../farbherd/src

LDFLAGS += -lzstd

LIBS = sdl2
LDFLAGS += $(shell pkg-config --libs $(LIBS))
CPPFLAGS += $(shell pkg-config --cflags $(LIBS))

DESTDIR ?= /usr/local

BINS=fhplayer

all: $(BINS)

fhplayer: $(DEP) src/fhplayer.c src/fhstream.c src/fhstream.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o fhplayer src/fhplayer.c src/fhstream.c

.PHONY:
install: $(BINS)
	mkdir -p $(DESTDIR)/bin
	install $(BINS) $(DESTDIR)/bin
clean:
	rm -f $(BINS)
