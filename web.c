#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "upnp.h"

#include "wphoto.h"

struct url_mapping {
	const char *url;
	const char *doc;
	size_t len;
};

static struct url_mapping url_maps[3];

static const struct url_mapping * resolve_get_url(const char *url)
{
	const struct url_mapping *map;
	const char *q;

	if (!(q = strchr(url, '?')))
		q = strchr(url, '\0');
	for (map = url_maps; map->url; map++) {
		if (strncmp(map->url, url, q - url) == 0)
			break;
	}
	return map;
}

static int upnp_web_getinfo(const char *filename,
#if UPNP_VERSION < 10800
		struct File_Info *info
#else
		UpnpFileInfo *info
#endif
		)
{
	const struct url_mapping *map;

	fprintf(stderr, "upnp_web_getinfo(\"%s\")\n", filename);
	if (!(map = resolve_get_url(filename)))
		return UPNP_E_FILE_NOT_FOUND;
	info->file_length = map->len;
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
	const struct url_mapping *map;

	fprintf(stderr, "upnp_web_open(\"%s\", %d)\n", filename, mode);
	if (mode != UPNP_READ)
		return NULL;
	map = resolve_get_url(filename);
	if (!map)
		return NULL;
	res = malloc(sizeof *res);
	if (!res)
		return NULL;
	res->doc = map->doc;
	res->len = map->len;
	res->pos = 0;
	return res;
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

int web_init(void)
{
	int i, ret;

	if (init_xml_docs() < 0) {
		printf("init_xml_docs error");
		return -1;
	}
	url_maps[0].url = "/MobileDevDesc.xml";
	url_maps[0].doc = xml_MobileDevDesc;
	url_maps[1].url = "/desc_iml/CameraConnectedMobile.xml";
	url_maps[1].doc = xml_CameraConnectedMobile;
	url_maps[2].url = NULL;
	url_maps[2].doc = NULL;
	for (i = 0; i < 2; i++)
		url_maps[i].len = strlen(url_maps[i].doc);

	ret = UpnpEnableWebserver(TRUE);
	if (ret != UPNP_E_SUCCESS) {
		printf("UpnpEnableWebserver error: %d\n", ret);
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

	return 0;
}
