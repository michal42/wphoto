all: wphoto

CFLAGS = $(shell pkg-config --cflags libupnp)
LDFLAGS = $(shell pkg-config --libs libupnp)
wphoto: wphoto.c
	$(CC) $(CFLAGS) $(LDFLAGS) wphoto.c -o wphoto
