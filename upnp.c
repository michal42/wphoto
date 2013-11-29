#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

#include "upnptools.h"
#include "upnp.h"

#include "wphoto.h"

const char *server_ip, *device_uuid;
int server_port;
static UpnpDevice_Handle device_handle = -1;
static UpnpClient_Handle client_handle = -1;

static void upnp_perror(const char *message, int err)
{
	fprintf(stderr, "%s: %s (%d)\n", message, UpnpGetErrorMessage(err), err);
};

enum discovery_state {
	DISCOVERY_NOTSTARTED,
	DISCOVERY_RUNNING,
	DISCOVERY_TIMEOUT,
	DISCOVERY_FOUND,
} device_dstate, client_dstate;

static int upnp_device_event_handler(Upnp_EventType type, void *event,
		void *cookie)
{
	fprintf(stderr, "device event: %d, %p, %p\n", type, event, cookie);
	return 0;
}

static int upnp_client_event_handler(Upnp_EventType type, void *event,
		void *cookie)
{
	struct Upnp_Discovery *devent = event;

	/* ignore our own advertisements */
	switch (type) {
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	case UPNP_DISCOVERY_SEARCH_RESULT:
		if (strcmp(devent->DeviceId, device_uuid) == 0) {
			// printf("Ignoring own advertisement\n");
			return 0;
		}
		break;
	default:
		;
	}

	switch (type) {
	case UPNP_DISCOVERY_SEARCH_TIMEOUT:
		client_dstate = DISCOVERY_TIMEOUT;
		break;
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	case UPNP_DISCOVERY_SEARCH_RESULT:
		printf("discovery event: %d\n", type);
		upnp_perror("discovery error code", devent->ErrCode);
		printf("Expires     =  %d\n",  devent->Expires);
		printf("DeviceId    =  %s\n",  devent->DeviceId);
		printf("DeviceType  =  %s\n",  devent->DeviceType);
		printf("ServiceType =  %s\n",  devent->ServiceType);
		printf("ServiceVer  =  %s\n",  devent->ServiceVer);
		printf("Location    =  %s\n",  devent->Location);
		printf("OS          =  %s\n",  devent->Os);
		printf("Ext         =  %s\n",  devent->Ext);
		break;
	default:
		fprintf(stderr, "unhandled client event: %d, %p, %p\n", type, event, cookie);
	}
	return 0;
}

static const char *web_MobileDevDesc(void *data, const char *query)
{
	return xml_MobileDevDesc;
}

static const char *web_CameraConnectedMobile(void *data, const char *query)
{
	return xml_CameraConnectedMobile;
}

struct stat dummy_st;
#define MARK(x) do { stat((x), &dummy_st); } while (0)

int wphoto_upnp_handshake(void)
{
	int ret = -1, err;
	char descurl[256];
	const char *desc_xml = "MobileDevDesc.xml";

	err = UpnpInit(NULL, 0);
	if (err != UPNP_E_SUCCESS) {
		printf("UpnpInit error: %d\n", err);
		goto err_init;
	}
	server_ip = UpnpGetServerIpAddress();
	server_port = UpnpGetServerPort();
	device_uuid = get_uuid();
	if (init_xml_docs() < 0) {
		printf("init_xml_docs error");
		goto err_init;
	}

	printf("address: %s:%d\n", server_ip, server_port);

	snprintf(descurl, sizeof(descurl), "http://%s:%d/%s",
			server_ip, server_port, desc_xml);
	err = web_add_callback("/MobileDevDesc.xml", web_MobileDevDesc, NULL);
	if (err) {
		perror("web_add_callback");
		goto err_init;
	}
	err = web_add_callback("/desc_iml/CameraConnectedMobile.xml",
			web_CameraConnectedMobile, NULL);
	if (err) {
		perror("web_add_callback");
		goto err_init;
	}
	if (web_start() < 0) {
		printf("web_init error\n");
		goto err_init;
	}
	err = UpnpRegisterRootDevice(descurl, upnp_device_event_handler,
			&device_handle, &device_handle);
	if (err != UPNP_E_SUCCESS) {
		printf("UpnpRegisterRootDevice error: %d\n", err);
		goto err_init;
	}
	err = UpnpRegisterClient(upnp_client_event_handler,
			&client_handle, &client_handle);
	if (err != UPNP_E_SUCCESS) {
		printf("UpnpRegisterClient error: %d\n", err);
		goto err_register;
	}
	do {
		MARK("/x1");
		err = UpnpSendAdvertisement(device_handle, 0);
		MARK("/x2");
		if (err != UPNP_E_SUCCESS) {
			printf("UpnpSendAdvertisement error: %d\n", err);
			goto err_register;
		}
		device_dstate = DISCOVERY_RUNNING;
		printf("NOTIFY sent\n");
		switch (client_dstate) {
		case DISCOVERY_RUNNING:
		case DISCOVERY_FOUND:
			sleep(3);
			continue;
		case DISCOVERY_TIMEOUT:
			printf("M-SEARCH timeout\n");
		default:
			;
		}
		client_dstate = DISCOVERY_RUNNING;
		err = UpnpSearchAsync(client_handle, 5, "urn:schemas-canon-com:service:MobileConnectedCameraService:1", (void*)42);
		if (err != UPNP_E_SUCCESS) {
			printf("UpnpSearchAsync error: %d\n", err);
			goto err_register;
		}
		printf("M-SEARCH sent\n");
		sleep(3);
	} while (device_dstate != DISCOVERY_FOUND && client_dstate != DISCOVERY_FOUND);
	ret = 0;
err_register:
	UpnpUnRegisterRootDevice(device_handle);
	MARK("/x4");
err_init:
	UpnpFinish();
	MARK("/x5");
	return ret;
}
