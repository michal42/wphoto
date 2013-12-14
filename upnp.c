#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include "upnptools.h"
#include "ithread.h"
#include "upnp.h"

#include "wphoto.h"

const char *server_ip;
int server_port;
static UpnpDevice_Handle device_handle = -1;
static UpnpClient_Handle client_handle = -1;

/*
 * This is used to signal the main thread of stage changes of the device
 * or client. The mutex protects all the three variables.
 */
ithread_mutex_t state_mutex;
ithread_cond_t state_cond;
/* URL of the discovered camera device */
char *camera_url;
/* camera issued a GET request to our device */
int camera_responded;
/* M-SEARCH timed out */
int discovery_timeout;

#define ADVERTISEMENT_INTERVAL 3
#define MSEARCH_INTERVAL 5
#define CAMERA_SERVICE_NAME "urn:schemas-canon-com:service:MobileConnectedCameraService:1"

void upnp_perror(const char *message, int err)
{
	fprintf(stderr, "%s: %s (%d)\n", message, UpnpGetErrorMessage(err), err);
};

/* strcmp() that accepts NULL pointers */
static int strcmp_null(const char *s1, const char *s2)
{
	if (!s1 || !s2)
		return !!s1 - !!s2;
	return strcmp(s1, s2);
}

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

	/* ignore devices other than the camera */
	switch (type) {
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	case UPNP_DISCOVERY_SEARCH_RESULT:
		if (strcmp(devent->ServiceType, CAMERA_SERVICE_NAME) != 0)
			return 0;
		break;
	default:
		;
	}

	switch (type) {
	case UPNP_DISCOVERY_SEARCH_TIMEOUT:
		ithread_mutex_lock(&state_mutex);
		discovery_timeout = 1;
		ithread_cond_signal(&state_cond);
		ithread_mutex_unlock(&state_mutex);
		break;
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
	case UPNP_DISCOVERY_SEARCH_RESULT:
		printf("discovery event: %d\n", type);
		ithread_mutex_lock(&state_mutex);
		if (strcmp_null(camera_url, devent->Location) != 0) {
			free(camera_url);
			camera_url = strdup(devent->Location);
			ithread_cond_signal(&state_cond);
		}
		ithread_mutex_unlock(&state_mutex);
		break;
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		fprintf(stderr, "FIXME: camera disconnected\n");
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
	ithread_mutex_lock(&state_mutex);
	camera_responded = 1;
	ithread_cond_signal(&state_cond);
	ithread_mutex_unlock(&state_mutex);
	return xml_CameraConnectedMobile;
}

static int ping_camera(const char *url)
{
	const char *cp;
	char *url2, *p;
	static const char *uuid;
	static size_t uuid_len;
	static const char path[] = "/desc_iml/MobileConnectedCamera.xml?uuid=";
	size_t base_len, len;
	char *outbuf, contenttype[LINE_SIZE];
	int err;

	/* Find the http://host:port part of the url */
	cp = strstr(url, "://");
	if (!cp) {
		fprintf(stderr, "Invalid device URL: %s\n", url);
		abort();
		return -1;
	}
	cp += 3;
	cp = strchr(cp, '/');
	if (!cp)
		cp = url + strlen(url);
	base_len = cp - url;
	if (base_len > 1024) {
		fprintf(stderr, "Device URL too long: %s\n", url);
		return -1;
	}
	if (!uuid) {
		uuid = get_uuid();
		uuid_len = strlen(uuid);
	}
	/* paste baseurl + path + uuid */
	len = base_len + sizeof(path) - 1 + uuid_len;
	url2 = malloc(len + 1);
	if (!url2) {
		perror("Memory allocation failure");
		return -1;
	}
	url2[len] = '\0';
	memcpy(url2, url, cp - url);
	p = url2 + (cp - url);
	memcpy(p, path, sizeof(path) - 1);
	p += sizeof(path) - 1;
	memcpy(p, uuid, uuid_len);
	fprintf(stderr, "url2: \"%s\"\n", url2);
	outbuf = NULL;
	err = UpnpDownloadUrlItem(url2, &outbuf, contenttype);
	/* FIXME: Check that the XML contains what we expect */
	free(outbuf);
	if (err)
		upnp_perror("UpnpDownloadUrlItem", err);
	return err;
}

int wphoto_upnp_handshake(void)
{
	int ret = -1, err;
	char descurl[256];
	const char *desc_xml = "MobileDevDesc.xml";
	struct timespec timer;
	int camera_responded_save;
	char *camera_url_save;
	int pinged_camera;

	ithread_mutex_init(&state_mutex, NULL);
	ithread_cond_init(&state_cond, NULL);
	camera_url = NULL;
	camera_responded = 0;
	err = UpnpInit(NULL, 0);
	if (err != UPNP_E_SUCCESS) {
		upnp_perror("UpnpInit", err);
		goto err_init;
	}
	server_ip = UpnpGetServerIpAddress();
	server_port = UpnpGetServerPort();
	if (init_xml_docs() < 0) {
		perror("init_xml_docs");
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
		upnp_perror("UpnpRegisterRootDevice", err);
		goto err_init;
	}
	err = UpnpRegisterClient(upnp_client_event_handler,
			&client_handle, &client_handle);
	if (err != UPNP_E_SUCCESS) {
		upnp_perror("UpnpRegisterClient", err);
		goto err_register;
	}
	clock_gettime(CLOCK_REALTIME, &timer);
	discovery_timeout = 1;
	camera_responded_save = 0;
	camera_url_save = NULL;
	pinged_camera = 0;
	do {
		int wait_err;

		if (!camera_responded_save) {
			err = UpnpSendAdvertisement(device_handle, 0);
			if (err != UPNP_E_SUCCESS) {
				upnp_perror("UpnpSendAdvertisement", err);
				goto err_register;
			}
			printf("NOTIFY sent\n");
		}
		if (camera_url_save && !pinged_camera)
			if (ping_camera(camera_url_save) == 0)
				pinged_camera = 1;
		timer.tv_sec += ADVERTISEMENT_INTERVAL;
wait:
		ithread_mutex_lock(&state_mutex);
		wait_err = 0;
		while (camera_responded == camera_responded_save &&
				strcmp_null(camera_url, camera_url_save) == 0 &&
				!discovery_timeout && wait_err == 0)
			wait_err = ithread_cond_timedwait(
					&state_cond, &state_mutex, &timer);
		camera_responded_save = camera_responded;
		if (strcmp_null(camera_url, camera_url_save) != 0) {
			free(camera_url_save);
			camera_url_save = strdup(camera_url);
		}
		/*
		 * Once we have the camera url, we stop sending M-SEARCH
		 * requests
		 */
		if (discovery_timeout && !camera_url_save) {
			err = UpnpSearchAsync(client_handle, MSEARCH_INTERVAL,
					CAMERA_SERVICE_NAME, (void*)42);
			if (err != UPNP_E_SUCCESS) {
				upnp_perror("UpnpSearchAsync", err);
				goto err_register;
			}
			printf("M-SEARCH sent\n");
		}
		discovery_timeout = 0;
		ithread_mutex_unlock(&state_mutex);
		if (wait_err != ETIMEDOUT &&
				(!pinged_camera || !camera_responded_save))
			goto wait;
	} while (!pinged_camera || !camera_responded_save);
	return 0;
err_register:
	UpnpUnRegisterRootDevice(device_handle);
err_init:
	UpnpFinish();
	return ret;
}
