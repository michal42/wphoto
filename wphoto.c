#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

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

static int upnp_web_getinfo(const char *filename,
#if UPNP_VERSION < 10800
		struct File_Info *info
#else
		UpnpFileInfo *info
#endif
		)
{
	fprintf(stderr, "upnp_web_getinfo(\"%s\")\n", filename);
	info->file_length = -1;
	info->last_modified = time(NULL);
	info->is_directory = 0;
	info->is_readable = 1;
	info->content_type = ixmlCloneDOMString("text/xml");
	return UPNP_E_SUCCESS;
}

struct my_filehandle {
	const char *doc;
	size_t len, pos;
};

static UpnpWebFileHandle upnp_web_open(const char *filename,
					enum UpnpOpenFileMode mode)
{
	struct my_filehandle *res;

	fprintf(stderr, "upnp_web_open(\"%s\", %d)\n", filename, mode);
	if (strcmp(filename, "/MobileDevDesc.xml") == 0) {
		if (mode != UPNP_READ) {
			return NULL;
		}
		res = malloc(sizeof *res);
		if (!res)
			return NULL;
		res->doc = xml_MobileDevDesc;
		res->len = strlen(xml_MobileDevDesc);
		res->pos = 0;
		return res;
	}
	return NULL;
}

static int upnp_web_read(UpnpWebFileHandle file, char *buf, size_t buflen)
{
	struct my_filehandle *fh = file;

	fprintf(stderr, "upnp_web_read(%p)\n", file);

	if (fh->len - fh->pos < buflen)
		buflen = fh->len - fh->pos;
	if (buflen)
		memcpy(buf, fh->doc + fh->pos, buflen);
	fh->pos += buflen;
	return buflen;
}

int upnp_web_write(UpnpWebFileHandle file, char *buf, size_t buflen)
{
	fprintf(stderr, "upnp_web_write(%p)\n", file);
	return -1;
}
int upnp_web_close(UpnpWebFileHandle file)
{
	fprintf(stderr, "upnp_web_close(%p)\n", file);
	free(file);
	return UPNP_E_SUCCESS;
}

UpnpDevice_Handle device_handle = -1;

int main(int argc, char **argv)
{
	int ret;
	char descurl[256];
	const char *desc_xml = "MobileDevDesc.xml";
	const char *webroot = "web";

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
	ret = UpnpAddVirtualDir("/");
	if (ret != UPNP_E_SUCCESS) {
		printf("UpnpAddVirtualDir error: %d\n", ret);
		UpnpFinish();
		return 1;
	}
	UpnpVirtualDir_set_GetInfoCallback(upnp_web_getinfo);
	UpnpVirtualDir_set_OpenCallback(upnp_web_open);
	UpnpVirtualDir_set_ReadCallback(upnp_web_read);
	UpnpVirtualDir_set_WriteCallback(upnp_web_write);
	UpnpVirtualDir_set_CloseCallback(upnp_web_close);
	if (init_xml_docs() < 0) {
		printf("init_xml_docs error: %d\n", ret);
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

