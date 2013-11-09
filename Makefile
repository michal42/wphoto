all: wphoto

CFLAGS = -g -Wall $(shell pkg-config --cflags libupnp)
LDFLAGS = $(shell pkg-config --libs libupnp)
wphoto: wphoto.o xml.o
	$(CC) $(LDFLAGS) $^ -o wphoto

