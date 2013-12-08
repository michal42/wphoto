#ifndef _WPHOTO_H
#define _WPHOTO_H

/* upnp.c */
extern const char *server_ip, *device_uuid;
extern int server_port;
int wphoto_upnp_handshake(void);
void upnp_perror(const char *message, int err);

/* xml.c */
extern char *xml_MobileDevDesc;
extern char *xml_CameraConnectedMobile;
int init_xml_docs(void);

/* web.c */
typedef const char * (*web_callback)(void *data, const char *query);
int web_add_callback(const char *path, web_callback callback, void *data);
int web_start(void);

/* uuid.c */
const char * get_uuid(void);

#endif
