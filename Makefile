all: wphoto

CFLAGS = -g -Wall $(shell pkg-config --cflags libupnp)
LDFLAGS = $(shell pkg-config --libs libupnp)
wphoto: wphoto.o upnp.o xml.o web.o uuid.o
	$(CC) $(LDFLAGS) $^ -o wphoto

clean:
	rm -f wphoto *.o

%.o: %.c wphoto.h
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: all clean
