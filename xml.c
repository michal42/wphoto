#include <sys/utsname.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "wphoto.h"

char *xml_MobileDevDesc;
char *xml_CameraConnectedMobile =
"<?xml version=\"1.0\"?>\n"
"<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">\n"
"	<specVersion>\n"
"		<major>1</major>\n"
"		<minor>0</minor>\n"
"	</specVersion>\n"
"	<actionList xmlns:ns=\"urn:schemas-canon-com:schema-imink\">\n"
"		<action>\n"
"			<name>GetObjRecvCapability</name>\n"
"			<ns:X_actKind>Get</X_actKind>\n"
"			<ns:X_resourceName>ObjRecvCapability</X_resourceName>\n"
"		</action>\n"
"		<action>\n"
"			<name>SetUsecaseStatus</name>\n"
"			<ns:X_actKind>Set</X_actKind>\n"
"			<ns:X_resourceName>UsecaseStatus</X_resourceName>\n"
"		</action>\n"
"		<action>\n"
"			<name>SetSendObjInfo</name>\n"
"			<ns:X_actKind>Set</X_actKind>\n"
"			<ns:X_resourceName>SendObjInfo</X_resourceName>\n"
"		</action>\n"
"		<action>\n"
"			<name>SetObjData</name>\n"
"			<ns:X_actKind>Set</X_actKind>\n"
"			<ns:X_resourceName>ObjData</X_resourceName>\n"
"		</action>\n"
"		<action>\n"
"			<name>SetMovieExtProperty</name>\n"
"			<ns:X_actKind>Set</X_actKind>\n"
"			<ns:X_resourceName>MovieExtProperty</X_resourceName>\n"
"		</action>\n"
"	</actionList>\n"
"</scpd>\n";

int append(char **doc, size_t *pos, size_t *alloc, const char *string)
{
	size_t len = strlen(string);
	size_t new_alloc = *alloc;
	while (*pos + len + 1 > new_alloc)
		new_alloc *= 2;
	if (new_alloc > *alloc) {
		char *new_doc;
		new_doc = realloc(*doc, new_alloc);
		if (!new_doc)
			return -1;
		*doc = new_doc;
		*alloc = new_alloc;
	}
	memcpy(*doc + *pos, string, len + 1);
	*pos += len;
	return 0;
}

int init_xml_docs(void)
{
	char buf[10];
	struct utsname utsname;
	size_t pos, alloc;
	char *dot;

#define AP(str) do { \
	if (append(&xml_MobileDevDesc, &pos, &alloc, str) < 0) { \
		free(xml_MobileDevDesc); \
		return -1; \
	} \
} while (0)

	pos = 0;
	alloc = 1024;
	xml_MobileDevDesc = malloc(alloc);
	if (!xml_MobileDevDesc)
		return -1;
	xml_MobileDevDesc[0] = '\0';
	uname(&utsname);
	dot = strchr(utsname.nodename, '.');
	if (dot)
		*dot = '\0';

	AP(
"<?xml version=\"1.0\"?>\n"
"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
"<specVersion>\n"
"<major>1</major>\n"
"<minor>0</minor>\n"
"</specVersion>\n"
"<URLBase>http://");
	/* <ip>:<port> */
	AP(server_ip);
	AP(":");
	snprintf(buf, sizeof buf, "%d", server_port);
	AP(buf);
	AP(
"/</URLBase>\n"
"<device>\n"
"<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>\n"
"<friendlyName>");
	AP(utsname.nodename);
	AP(
"</friendlyName>\n"
"<manufacturer>Michal Marek</manufacturer>\n"
"<manufacturerURL>http://michal.markovi.net/</manufacturerURL>\n"
"<modelDescription>wphoto</modelDescription>\n"
"<modelName>n/a</modelName>\n"
"<UDN>");
	AP(device_uuid);
	AP(
"</UDN>\n"
"<serviceList>\n"
"<service>\n"
"<serviceType>urn:schemas-canon-com:service:CameraConnectedMobileService:1</serviceType>\n"
"<serviceId>urn:schemas-canon-com:serviceId:CameraConnectedMobile</serviceId>\n"
"<SCPDURL>desc/CameraConnectedMobile.xml</SCPDURL>\n"
"<controlURL>CameraConnectedMobile/</controlURL>\n"
"<eventSubURL> </eventSubURL>\n"
"<pnpx:X_SCPDURL xmlns:pnpx=\"urn:schemas-canon-com:schema-imink\">desc_iml/CameraConnectedMobile.xml</pnpx:X_SCPDURL>\n"
"<pnpx:X_ExtActionVer xmlns:pnpx=\"urn:schemas-canon-com:schema-imink\">1.0</pnpx:X_ExtActionVer>\n"
"<pnpx:X_VendorExtVer xmlns:pnpx=\"urn:schemas-canon-com:schema-imink\">1-1302.0.0.0</pnpx:X_VendorExtVer>\n"
"</service>\n"
"</serviceList>\n"
"<presentationURL>/</presentationURL>\n"
"</device>\n"
"</root>\n");

#undef AP
	return 0;
}
