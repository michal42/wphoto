#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "upnp.h"

#include "wphoto.h"

struct url_mapping {
	struct url_mapping *next;
	const char *path;
	web_callback callback;
	void *data;
} *url_maps;

int web_add_callback(const char *path, web_callback callback, void *data)
{
	struct url_mapping *new;

	new = malloc(sizeof *new);
	if (!new)
		return -1;
	new->path = path;
	new->callback = callback;
	new->data = data;
	new->next = url_maps;
	url_maps = new;
	return 0;
}

static const struct url_mapping * get_url_map(const char *url)
{
	const struct url_mapping *map;
	const char *q;

	if (!(q = strchr(url, '?')))
		q = strchr(url, '\0');
	for (map = url_maps; map; map = map->next) {
		if (strncmp(map->path, url, q - url) == 0)
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
	if (!(map = get_url_map(filename)))
		return UPNP_E_FILE_NOT_FOUND;
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
	const struct url_mapping *map;
	const char *doc, *query;

	fprintf(stderr, "upnp_web_open(\"%s\", %d)\n", filename, mode);
	if (mode != UPNP_READ)
		return NULL;
	map = get_url_map(filename);
	if (!map)
		return NULL;
	query = strchr(filename, '?');
	if (query)
		query++;
	doc = map->callback(map->data, query);
	if (!doc)
		return NULL;
	res = malloc(sizeof *res);
	if (!res)
		return NULL;
	res->doc = doc;
	res->len = strlen(doc);
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

int web_start(void)
{
	int ret;

	ret = UpnpEnableWebserver(TRUE);
	if (ret != UPNP_E_SUCCESS) {
		upnp_perror("UpnpEnableWebserver", ret);
		return 1;
	}
	ret = UpnpAddVirtualDir("/");
	if (ret != UPNP_E_SUCCESS) {
		upnp_perror("UpnpAddVirtualDir", ret);
		return 1;
	}
	UpnpVirtualDir_set_GetInfoCallback(upnp_web_getinfo);
	UpnpVirtualDir_set_OpenCallback(upnp_web_open);
	UpnpVirtualDir_set_ReadCallback(upnp_web_read);
	UpnpVirtualDir_set_WriteCallback(upnp_web_write);
	UpnpVirtualDir_set_CloseCallback(upnp_web_close);

	return 0;
}
