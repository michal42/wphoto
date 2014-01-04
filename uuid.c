#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "wphoto.h"

/*
 * We need to obtain an UUID that is persistent across reboots, does not
 * depend on the presence of removable hardware and ideally is the same for
 * all user accounts (to avoid surprises when running as root, plus there
 * can only be one instance of the imink server at a time). The various uuid
 * libraries have the opposite goal, hence this implementation. We do not care
 * about the format of the UUID, as the camera happily accept any hex string
 * in the 8-4-4-4-12 notation.
 */

#define UUID_LEN 16
#define UUID_STR_LEN 36

static char uuid_str[UUID_STR_LEN];
static int have_uuid_str;

/*
 * We use NULL as "not available" in the functions below, whereas OOM is a
 * temporary condition. Exit early to avoid possibly returning different UUIDs
 * under different memory conditions.
 */
static void * xmalloc(size_t len)
{
	void *res = malloc(len);

	if (!res) {
		perror("memory allocation failure");
		exit(1);
	}
	return res;
}

/*
 * Check if at least two bytes are not 0x00 or 0xff
 */
static int is_plausible_uuid(unsigned char *uuid)
{
	int i, good;

	for (i = 0, good = 0; i < UUID_LEN; i++) {
		if (uuid[i] == 0 || uuid[i] == 0xff)
			continue;
		if (++good >= 2)
			return 1;
	}
	return 0;
}

/*
 * Reads a hexadecimal uuid from <path> and returns its binary form. The caller
 * must free the returned uuid.
 */
static unsigned char * uuid_from_file(const char *path)
{
	unsigned char *res, *r;
	char *str, *s;
	FILE *f;
	int cur, prev;

	f = fopen(path, "r");
	if (!f)
		return NULL;
	res = xmalloc(UUID_LEN);
	memset(res, '\0', UUID_LEN);
	str = xmalloc(UUID_STR_LEN + 1);
	if (!fgets(str, UUID_STR_LEN, f)) {
		free(res);
		free(str);
		fclose(f);
		return NULL;
	}
	fclose(f);
	r = res + UUID_LEN - 1;
	prev = -1;
	/* Convert the string from the end, skipping non-hex characters */
	for (s = strchr(str, '\0'); s >= str && r >= res; s--) {
		if (*s >= '0' && *s <= '9')
			cur = *s - '0';
		else if (*s >= 'A' && *s <= 'F')
			cur = *s - 'A' + 10;
		else if (*s >= 'a' && *s <= 'f')
			cur = *s - 'a' + 10;
		else
			continue;
		if (prev == -1) {
			prev = cur;
			continue;
		}
		*r = (cur << 4) + prev;
		r--;
		prev = -1;
	}
	free(str);
	if (!is_plausible_uuid(res)) {
		free(res);
		return NULL;
	}
	return res;
}

/*
 * If /sys/class/net/<name> points to ../devices/virtual or ../devices/.../usb,
 * we consider the device transient and try a better one if available. This
 * should skip usb modems / network cards and things like virtual machine
 * interfaces or VPN endpoints.
 */
static int is_transient_interface(const char *name)
{
	char path[256], target[256];
	ssize_t size;

	snprintf(path, sizeof(path), "/sys/class/net/%s", name);
	size = readlink(path, target, sizeof(target) - 1);
	if (size < 0) {
		fprintf(stderr, "warning: cannot read link target of %s: %s\n",
				path, strerror(errno));
		return 1;
	}
	target[size] = '\0';
	if (!strstr(target, "/devices/")) {
		fprintf(stderr, "warning: %s points to an unexpected target: %s\n",
				path, target);
		return 1;
	}
	if (strstr(target, "/usb"))
		return 1;
	if (strstr(target, "/devices/virtual/"))
		return 1;
	return 0;
}

static unsigned char * linux_mac_uuid(void)
{
	DIR *dir;
	struct dirent *de;
	unsigned char *candidate1 = NULL, *candidate2 = NULL;
	int transient;


	if (!(dir = opendir("/sys/class/net")))
		return NULL;
	while ((de = readdir(dir))) {
		unsigned char **candidate;
		unsigned char *new_uuid;
		char path[256];

		if (de->d_name[0] == '.')
			continue;
		transient = is_transient_interface(de->d_name);
		if (transient && candidate1)
			continue;
		candidate = transient ? &candidate2 : &candidate1;
		snprintf(path, sizeof(path), "/sys/class/net/%s/address",
				de->d_name);
		new_uuid = uuid_from_file(path);
		if (!new_uuid)
			continue;
		if (*candidate && memcmp(*candidate, new_uuid, UUID_LEN) < 0) {
			free(new_uuid);
			continue;
		}
		free(*candidate);
		*candidate = new_uuid;
	}
	closedir(dir);
	if (candidate1) {
		free(candidate2);
		return candidate1;
	}
	return candidate2;
}

static unsigned char * fallback_uuid(void)
{
	unsigned char *res = xmalloc(UUID_LEN);
	int i;

	for (i = 0; i < UUID_LEN; i++)
		res[i] = i + 1;

	return res;
}

static void uuid_to_str(char *uuid_str, const unsigned char *uuid)
{
	char *p;
	int i;

	p = uuid_str;
	for (i = 0; i < UUID_LEN; i++) {
		sprintf(p, "%02X", uuid[i]);
		p += 2;
		if (i == 4 || i == 6 || i == 8 || i == 10) {
			*p++ = '-';
		}
	}
	uuid_str[UUID_STR_LEN] = '\0';
}

static unsigned char * get_uuid_bin(void)
{
	unsigned char *res;

	if ((res = linux_mac_uuid()))
		return res;
	return fallback_uuid();
}

const char * get_uuid(void)
{
	unsigned char *uuid;

	if (have_uuid_str)
		return uuid_str;
	uuid = get_uuid_bin();
	uuid_to_str(uuid_str, uuid);
	free(uuid);
	return uuid_str;
}

#ifdef UUID_TEST

int main(int argc, char **argv)
{
	unsigned char *uuid;
	char uuid_str[UUID_STR_LEN];

	printf("get_uuid()          : %s\n", get_uuid());
	uuid = linux_mac_uuid();
	if (uuid) {
		uuid_to_str(uuid_str, uuid);
		free(uuid);
		printf("linux_mac_uuid()    : %s\n", uuid_str);
	}
	while (*++argv) {
		uuid = uuid_from_file(*argv);
		if (!uuid)
			continue;
		uuid_to_str(uuid_str, uuid);
		free(uuid);
		printf("%-20s: %s\n", *argv, uuid_str);
	}
	return 0;
}
#endif
