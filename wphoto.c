#include <unistd.h>
#include <stdio.h>

#include "wphoto.h"


int main(int argc, char **argv)
{
	int err;

	err = wphoto_upnp_handshake();
	if (err < 0) {
		fprintf(stderr, "Failed to connect to camera\n");
		return 1;
	}
	printf("TODO\n");
	sleep(300);
	return 0;
}

