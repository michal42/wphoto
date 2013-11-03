#include <stdio.h>

#include "upnp.h"

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
	const char *ip;
	int port;
	char descurl[256];
	const char *desc_xml = "MobileDevDesc.xml";
	const char *webroot = "web";

	ret = UpnpInit(NULL, 0);
	if (ret != UPNP_E_SUCCESS) {
		printf("UpnpInit error: %d\n", ret);
		UpnpFinish();
		return 1;
	}
	ip = UpnpGetServerIpAddress();
	port = UpnpGetServerPort();

	printf("address: %s:%d\n", ip, port);

	snprintf(descurl, sizeof(descurl), "http://%s:%d/%s",
			ip, port, desc_xml);
	ret = UpnpSetWebServerRootDir(webroot);
	if (ret != UPNP_E_SUCCESS) {
		printf("UpnpSetWebServerRootDir error: %d\n", ret);
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
	sleep(10);
	UpnpUnRegisterRootDevice(device_handle);
	UpnpFinish();
	return 0;
}

