#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "upnp.h"

#include "wphoto.h"

const char *server_ip;
int server_port;

static int upnp_event_handler(Upnp_EventType type, void *event,
		void *cookie)
{
	fprintf(stderr, "event: %d, %p, %p\n", type, event, cookie);
	return 0;
}


UpnpDevice_Handle device_handle = -1;
UpnpClient_Handle client_handle = -1;
struct stat dummy_st;
#define MARK(x) do { stat((x), &dummy_st); } while (0)

int main(int argc, char **argv)
{
	int ret = 1, err;
	char descurl[256];
	const char *desc_xml = "MobileDevDesc.xml";

	err = UpnpInit(NULL, 0);
	if (err != UPNP_E_SUCCESS) {
		printf("UpnpInit error: %d\n", err);
		goto err_init;
	}
	server_ip = UpnpGetServerIpAddress();
	server_port = UpnpGetServerPort();

	printf("address: %s:%d\n", server_ip, server_port);

	snprintf(descurl, sizeof(descurl), "http://%s:%d/%s",
			server_ip, server_port, desc_xml);
	if (web_init() < 0) {
		printf("web_init error\n");
		goto err_init;
	}
	err = UpnpRegisterRootDevice(descurl, upnp_event_handler,
			&device_handle, &device_handle);
	if (err != UPNP_E_SUCCESS) {
		printf("UpnpRegisterRootDevice error: %d\n", err);
		goto err_init;
	}
	err = UpnpRegisterClient(upnp_event_handler,
			&client_handle, &client_handle);
	if (err != UPNP_E_SUCCESS) {
		printf("UpnpRegisterClient error: %d\n", err);
		goto err_register;
	}
	MARK("/x1");
	err = UpnpSendAdvertisement(device_handle, 0);
	MARK("/x2");
	if (err != UPNP_E_SUCCESS) {
		printf("UpnpSendAdvertisement error: %d\n", err);
		goto err_register;
	}
	err = UpnpSearchAsync(client_handle, 5, "urn:schemas-canon-com:service:MobileConnectedCameraService:1", (void*)42);
	if (err != UPNP_E_SUCCESS) {
		printf("UpnpSearchAsync error: %d\n", err);
		goto err_register;
	}
	sleep(100);
	MARK("/x3");
	ret = 0;
err_register:
	UpnpUnRegisterRootDevice(device_handle);
	MARK("/x4");
err_init:
	UpnpFinish();
	MARK("/x5");
	return ret;
}

