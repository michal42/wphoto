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
	fprintf(stderr, "event: %d\n", type);
	return 0;
}


UpnpDevice_Handle device_handle = -1;

int main(int argc, char **argv)
{
	int ret;
	char descurl[256];
	const char *desc_xml = "MobileDevDesc.xml";

	ret = UpnpInit(NULL, 0);
	if (ret != UPNP_E_SUCCESS) {
		printf("UpnpInit error: %d\n", ret);
		UpnpFinish();
		return 1;
	}
	server_ip = UpnpGetServerIpAddress();
	server_port = UpnpGetServerPort();

	printf("address: %s:%d\n", server_ip, server_port);

	snprintf(descurl, sizeof(descurl), "http://%s:%d/%s",
			server_ip, server_port, desc_xml);
	if (init_xml_docs() < 0) {
		printf("init_xml_docs error: %d\n", ret);
		UpnpFinish();
		return 1;
	}
	if (web_init() < 0) {
		printf("web_init error\n");
		UpnpFinish();
		return 1;
	}
	ret = UpnpRegisterRootDevice(descurl, upnp_event_handler,
			&device_handle, &device_handle);
	if (ret != UPNP_E_SUCCESS) {
		printf("UpnpRegisterRootDevice error: %d\n", ret);
		UpnpFinish();
		return 1;
	}
	ret = UpnpSendAdvertisement(device_handle, 0);
	if (ret != UPNP_E_SUCCESS) {
		printf("UpnpSendAdvertisement error: %d\n", ret);
		UpnpUnRegisterRootDevice(device_handle);
		UpnpFinish();
		return 1;
	}
	sleep(100);
	UpnpUnRegisterRootDevice(device_handle);
	UpnpFinish();
	return 0;
}

