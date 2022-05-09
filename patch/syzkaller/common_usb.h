// Copyright 2019 syzkaller project authors. All rights reserved.
// Use of this source code is governed by Apache 2 LICENSE that can be found in the LICENSE file.

// This file is shared between executor and csource package.

// Implementation of syz_usb_* pseudo-syscalls.

#define USB_DEBUG 0

#define USB_MAX_IFACE_NUM 4
#define USB_MAX_EP_NUM 32

struct usb_iface_index {
	struct usb_interface_descriptor* iface;
	uint8 bInterfaceNumber;
	uint8 bAlternateSetting;
	struct usb_endpoint_descriptor eps[USB_MAX_EP_NUM];
	int eps_num;
};

struct usb_device_index {
	struct usb_device_descriptor* dev;
	struct usb_config_descriptor* config;
	uint8 bMaxPower;
	int config_length;
	struct usb_iface_index ifaces[USB_MAX_IFACE_NUM];
	int ifaces_num;
	int iface_cur;
};

struct configfs_attr {
	uint16 bcdUSB;
	uint8 bDeviceClass;
	//	uint8  bDeviceSubClass;
	//	uint8  bDeviceProtocol;
	//	uint8  bMaxPacketSize0;
	uint16 idVendor;
	uint16 idProduct;
	uint16 bcdDevice;
	//	uint8  iManufacturer;
	//	uint8  iProduct;
	//	uint8  iSerialNumber;
	//	uint8 bMaxPower;
};

static bool parse_usb_descriptor(char* buffer, size_t length, struct usb_device_index* index)
{
	if (length < sizeof(*index->dev) + sizeof(*index->config))
		return false;

	memset(index, 0, sizeof(*index));

	index->dev = (struct usb_device_descriptor*)buffer;
	index->config = (struct usb_config_descriptor*)(buffer + sizeof(*index->dev));
	index->bMaxPower = index->config->bMaxPower;
	index->config_length = length - sizeof(*index->dev);
	index->iface_cur = -1;
	size_t offset = 0;

	while (true) {
		if (offset + 1 >= length)
			break;
		uint8 desc_length = buffer[offset];
		uint8 desc_type = buffer[offset + 1];
		if (desc_length <= 2)
			break;
		if (offset + desc_length > length)
			break;
		if (desc_type == USB_DT_INTERFACE && index->ifaces_num < USB_MAX_IFACE_NUM) {
			struct usb_interface_descriptor* iface = (struct usb_interface_descriptor*)(buffer + offset);
			debug("parse_usb_descriptor: found interface #%u (%d, %d) at %p\n",
			      index->ifaces_num, iface->bInterfaceNumber, iface->bAlternateSetting, iface);
			index->ifaces[index->ifaces_num].iface = iface;
			index->ifaces[index->ifaces_num].bInterfaceNumber = iface->bInterfaceNumber;
			index->ifaces[index->ifaces_num].bAlternateSetting = iface->bAlternateSetting;
			index->ifaces_num++;
		}
		if (desc_type == USB_DT_ENDPOINT && index->ifaces_num > 0) {
			struct usb_iface_index* iface = &index->ifaces[index->ifaces_num - 1];
			debug("parse_usb_descriptor: found endpoint #%u at %p\n", iface->eps_num, buffer + offset);
			if (iface->eps_num < USB_MAX_EP_NUM) {
				memcpy(&iface->eps[iface->eps_num], buffer + offset, sizeof(iface->eps[iface->eps_num]));
				iface->eps_num++;
			}
		}
		offset += desc_length;
	}

	return true;
}

struct usb_raw_init {
	__u64 speed;
	const __u8* driver_name;
	const __u8* device_name;
};

enum usb_raw_event_type {
	USB_RAW_EVENT_INVALID,
	USB_RAW_EVENT_CONNECT,
	USB_RAW_EVENT_CONTROL,
};

struct usb_raw_event {
	__u32 type;
	__u32 length;
	__u8 data[0];
};

struct usb_raw_ep_io {
	__u16 ep;
	__u16 flags;
	__u32 length;
	__u8 data[0];
};

#define USB_RAW_IOCTL_INIT _IOW('U', 0, struct usb_raw_init)
#define USB_RAW_IOCTL_RUN _IO('U', 1)
#define USB_RAW_IOCTL_EVENT_FETCH _IOR('U', 2, struct usb_raw_event)
#define USB_RAW_IOCTL_EP0_WRITE _IOW('U', 3, struct usb_raw_ep_io)
#define USB_RAW_IOCTL_EP0_READ _IOWR('U', 4, struct usb_raw_ep_io)
#define USB_RAW_IOCTL_EP_ENABLE _IOW('U', 5, struct usb_endpoint_descriptor)
#define USB_RAW_IOCTL_EP_DISABLE _IOW('U', 6, __u32)
#define USB_RAW_IOCTL_EP_WRITE _IOW('U', 7, struct usb_raw_ep_io)
#define USB_RAW_IOCTL_EP_READ _IOWR('U', 8, struct usb_raw_ep_io)
#define USB_RAW_IOCTL_CONFIGURE _IO('U', 9)
#define USB_RAW_IOCTL_VBUS_DRAW _IOW('U', 10, __u32)

static int usb_raw_open()
{
	return open("/dev/raw-gadget", O_RDWR);
}

static int usb_raw_init(int fd, uint32 speed, const char* driver, const char* device)
{
	struct usb_raw_init arg;
	arg.speed = speed;
	arg.driver_name = (const __u8*)driver;
	arg.device_name = (const __u8*)device;
	return ioctl(fd, USB_RAW_IOCTL_INIT, &arg);
}

static int usb_raw_run(int fd)
{
	return ioctl(fd, USB_RAW_IOCTL_RUN, 0);
}

static int usb_raw_event_fetch(int fd, struct usb_raw_event* event)
{
	return ioctl(fd, USB_RAW_IOCTL_EVENT_FETCH, event);
}

static int usb_raw_ep0_write(int fd, struct usb_raw_ep_io* io)
{
	return ioctl(fd, USB_RAW_IOCTL_EP0_WRITE, io);
}

static int usb_raw_ep0_read(int fd, struct usb_raw_ep_io* io)
{
	return ioctl(fd, USB_RAW_IOCTL_EP0_READ, io);
}

#if SYZ_EXECUTOR || __NR_syz_usb_ep_write
static int usb_raw_ep_write(int fd, struct usb_raw_ep_io* io)
{
	return ioctl(fd, USB_RAW_IOCTL_EP_WRITE, io);
}
#endif

#if SYZ_EXECUTOR || __NR_syz_usb_ep_read
static int usb_raw_ep_read(int fd, struct usb_raw_ep_io* io)
{
	return ioctl(fd, USB_RAW_IOCTL_EP_READ, io);
}
#endif

static int usb_raw_ep_enable(int fd, struct usb_endpoint_descriptor* desc)
{
	return ioctl(fd, USB_RAW_IOCTL_EP_ENABLE, desc);
}

static int usb_raw_ep_disable(int fd, int ep)
{
	return ioctl(fd, USB_RAW_IOCTL_EP_DISABLE, ep);
}

static int usb_raw_configure(int fd)
{
	return ioctl(fd, USB_RAW_IOCTL_CONFIGURE, 0);
}

static int usb_raw_vbus_draw(int fd, uint32 power)
{
	return ioctl(fd, USB_RAW_IOCTL_VBUS_DRAW, power);
}

#define MAX_USB_FDS 6

struct usb_info {
	int fd;
	struct usb_device_index index;
};

static struct usb_info usb_devices[MAX_USB_FDS];
static int usb_devices_num;

static struct usb_device_index* add_usb_index(int fd, char* dev, size_t dev_len)
{
	int i = __atomic_fetch_add(&usb_devices_num, 1, __ATOMIC_RELAXED);
	if (i >= MAX_USB_FDS)
		return NULL;

	int rv = 0;
	NONFAILING(rv = parse_usb_descriptor(dev, dev_len, &usb_devices[i].index));
	if (!rv)
		return NULL;

	__atomic_store_n(&usb_devices[i].fd, fd, __ATOMIC_RELEASE);
	return &usb_devices[i].index;
}

static struct usb_device_index* lookup_usb_index(int fd)
{
	int i;
	for (i = 0; i < MAX_USB_FDS; i++) {
		if (__atomic_load_n(&usb_devices[i].fd, __ATOMIC_ACQUIRE) == fd) {
			return &usb_devices[i].index;
		}
	}
	return NULL;
}

#if SYZ_EXECUTOR || __NR_syz_usb_control_io
static int lookup_interface(int fd, uint8 bInterfaceNumber, uint8 bAlternateSetting)
{
	struct usb_device_index* index = lookup_usb_index(fd);
	int i;

	if (!index)
		return -1;

	for (i = 0; i < index->ifaces_num; i++) {
		if (index->ifaces[i].bInterfaceNumber == bInterfaceNumber &&
		    index->ifaces[i].bAlternateSetting == bAlternateSetting)
			return i;
	}
	return -1;
}
#endif

static void set_interface(int fd, int n)
{
	struct usb_device_index* index = lookup_usb_index(fd);
	int ep;

	if (!index)
		return;

	if (index->iface_cur >= 0 && index->iface_cur < index->ifaces_num) {
		for (ep = 0; ep < index->ifaces[index->iface_cur].eps_num; ep++) {
			int rv = usb_raw_ep_disable(fd, ep);
			if (rv < 0) {
				debug("set_interface: failed to disable endpoint %d\n", ep);
			} else {
				debug("set_interface: endpoint %d disabled\n", ep);
			}
		}
	}
	if (n >= 0 && n < index->ifaces_num) {
		for (ep = 0; ep < index->ifaces[n].eps_num; ep++) {
			int rv = usb_raw_ep_enable(fd, &index->ifaces[n].eps[ep]);
			if (rv < 0) {
				debug("set_interface: failed to enable endpoint %d\n", ep);
			} else {
				debug("set_interface: endpoint %d enabled as %d\n", ep, rv);
			}
		}
		index->iface_cur = n;
	}
}

static int configure_device(int fd)
{
	struct usb_device_index* index = lookup_usb_index(fd);

	if (!index)
		return -1;

	int rv = usb_raw_vbus_draw(fd, index->bMaxPower);
	if (rv < 0) {
		debug("configure_device: usb_raw_vbus_draw failed with %d\n", rv);
		return rv;
	}
	rv = usb_raw_configure(fd);
	if (rv < 0) {
		debug("configure_device: usb_raw_configure failed with %d\n", rv);
		return rv;
	}
	set_interface(fd, 0);
	return 0;
}

#define USB_MAX_PACKET_SIZE 1024

struct usb_raw_control_event {
	struct usb_raw_event inner;
	struct usb_ctrlrequest ctrl;
	char data[USB_MAX_PACKET_SIZE];
};

struct usb_raw_ep_io_data {
	struct usb_raw_ep_io inner;
	char data[USB_MAX_PACKET_SIZE];
};

struct vusb_connect_string_descriptor {
	uint32 len;
	char* str;
} __attribute__((packed));

struct vusb_connect_descriptors {
	uint32 qual_len;
	char* qual;
	uint32 bos_len;
	char* bos;
	uint32 strs_len;
	struct vusb_connect_string_descriptor strs[0];
} __attribute__((packed));

static const char default_string[] = {
    8, USB_DT_STRING,
    's', 0, 'y', 0, 'z', 0 // UTF16-encoded "syz"
};

static const char default_lang_id[] = {
    4, USB_DT_STRING,
    0x09, 0x04 // English (United States)
};

static bool lookup_connect_response(int fd, struct vusb_connect_descriptors* descs, struct usb_ctrlrequest* ctrl,
				    char** response_data, uint32* response_length)
{
	struct usb_device_index* index = lookup_usb_index(fd);
	uint8 str_idx;

	if (!index)
		return false;

	switch (ctrl->bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_STANDARD:
		switch (ctrl->bRequest) {
		case USB_REQ_GET_DESCRIPTOR:
			switch (ctrl->wValue >> 8) {
			case USB_DT_DEVICE:
				*response_data = (char*)index->dev;
				*response_length = sizeof(*index->dev);
				return true;
			case USB_DT_CONFIG:
				*response_data = (char*)index->config;
				*response_length = index->config_length;
				return true;
			case USB_DT_STRING:
				str_idx = (uint8)ctrl->wValue;
				if (descs && str_idx < descs->strs_len) {
					*response_data = descs->strs[str_idx].str;
					*response_length = descs->strs[str_idx].len;
					return true;
				}
				if (str_idx == 0) {
					*response_data = (char*)&default_lang_id[0];
					*response_length = default_lang_id[0];
					return true;
				}
				*response_data = (char*)&default_string[0];
				*response_length = default_string[0];
				return true;
			case USB_DT_BOS:
				*response_data = descs->bos;
				*response_length = descs->bos_len;
				return true;
			case USB_DT_DEVICE_QUALIFIER:
				if (!descs->qual) {
					// Fill in DEVICE_QUALIFIER based on DEVICE if not provided.
					struct usb_qualifier_descriptor* qual =
					    (struct usb_qualifier_descriptor*)response_data;
					qual->bLength = sizeof(*qual);
					qual->bDescriptorType = USB_DT_DEVICE_QUALIFIER;
					qual->bcdUSB = index->dev->bcdUSB;
					qual->bDeviceClass = index->dev->bDeviceClass;
					qual->bDeviceSubClass = index->dev->bDeviceSubClass;
					qual->bDeviceProtocol = index->dev->bDeviceProtocol;
					qual->bMaxPacketSize0 = index->dev->bMaxPacketSize0;
					qual->bNumConfigurations = index->dev->bNumConfigurations;
					qual->bRESERVED = 0;
					*response_length = sizeof(*qual);
					return true;
				}
				*response_data = descs->qual;
				*response_length = descs->qual_len;
				return true;
			default:
				fail("lookup_connect_response: no response");
				return false;
			}
			break;
		default:
			fail("lookup_connect_response: no response");
			return false;
		}
		break;
	default:
		fail("lookup_connect_response: no response");
		return false;
	}

	return false;
}

unsigned char report_desc_keyboard[] = {
    0x05, 0x01, /* USAGE_PAGE (Generic Desktop)	          */
    0x09, 0x06, /* USAGE (Keyboard)                       */
    0xa1, 0x01, /* COLLECTION (Application)               */
    0x05, 0x07, /*   USAGE_PAGE (Keyboard)                */
    0x19, 0xe0, /*   USAGE_MINIMUM (Keyboard LeftControl) */
    0x29, 0xe7, /*   USAGE_MAXIMUM (Keyboard Right GUI)   */
    0x15, 0x00, /*   LOGICAL_MINIMUM (0)                  */
    0x25, 0x01, /*   LOGICAL_MAXIMUM (1)                  */
    0x75, 0x01, /*   REPORT_SIZE (1)                      */
    0x95, 0x08, /*   REPORT_COUNT (8)                     */
    0x81, 0x02, /*   INPUT (Data,Var,Abs)                 */
    0x95, 0x01, /*   REPORT_COUNT (1)                     */
    0x75, 0x08, /*   REPORT_SIZE (8)                      */
    0x81, 0x03, /*   INPUT (Cnst,Var,Abs)                 */
    0x95, 0x05, /*   REPORT_COUNT (5)                     */
    0x75, 0x01, /*   REPORT_SIZE (1)                      */
    0x05, 0x08, /*   USAGE_PAGE (LEDs)                    */
    0x19, 0x01, /*   USAGE_MINIMUM (Num Lock)             */
    0x29, 0x05, /*   USAGE_MAXIMUM (Kana)                 */
    0x91, 0x02, /*   OUTPUT (Data,Var,Abs)                */
    0x95, 0x01, /*   REPORT_COUNT (1)                     */
    0x75, 0x03, /*   REPORT_SIZE (3)                      */
    0x91, 0x03, /*   OUTPUT (Cnst,Var,Abs)                */
    0x95, 0x06, /*   REPORT_COUNT (6)                     */
    0x75, 0x08, /*   REPORT_SIZE (8)                      */
    0x15, 0x00, /*   LOGICAL_MINIMUM (0)                  */
    0x25, 0x65, /*   LOGICAL_MAXIMUM (101)                */
    0x05, 0x07, /*   USAGE_PAGE (Keyboard)                */
    0x19, 0x00, /*   USAGE_MINIMUM (Reserved)             */
    0x29, 0x65, /*   USAGE_MAXIMUM (Keyboard Application) */
    0x81, 0x00, /*   INPUT (Data,Ary,Abs)                 */
    0xc0 /* END_COLLECTION                         */
};

unsigned char report_desc_mouse[] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x02, // USAGE (Mouse)
    0xa1, 0x01, // COLLECTION (Application)
    0x09, 0x01, //   USAGE (Pointer)
    0xa1, 0x00, //   COLLECTION (Physical)
    0x05, 0x09, //     USAGE_PAGE (Button)
    0x19, 0x01, //     USAGE_MINIMUM (Button 1)
    0x29, 0x03, //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00, //     LOGICAL_MINIMUM (0)
    0x25, 0x01, //     LOGICAL_MAXIMUM (1)
    0x95, 0x03, //     REPORT_COUNT (3)
    0x75, 0x01, //     REPORT_SIZE (1)
    0x81, 0x02, //     INPUT (Data,Var,Abs)
    0x95, 0x01, //     REPORT_COUNT (1)
    0x75, 0x05, //     REPORT_SIZE (5)
    0x81, 0x03, //     INPUT (Cnst,Var,Abs)
    0x05, 0x01, //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30, //     USAGE (X)
    0x09, 0x31, //     USAGE (Y)
    0x15, 0x81, //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f, //     LOGICAL_MAXIMUM (127)
    0x75, 0x08, //     REPORT_SIZE (8)
    0x95, 0x02, //     REPORT_COUNT (2)
    0x81, 0x06, //     INPUT (Data,Var,Rel)
    0xc0, //   END_COLLECTION
    0xc0};

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
int sg_fd;
extern int errno;

#define configfs_set_descs(a)                             \
	char b[7];                                        \
	if ((a)->bDeviceClass) {                          \
		fd_gadget = open("bDeviceClass", O_RDWR); \
		sprintf(b, "0x%x", (a)->bDeviceClass);    \
		ret = write(fd_gadget, b, strlen(b));     \
		close(fd_gadget);                         \
	}                                                 \
	fd_gadget = open("idVendor", O_RDWR);             \
	sprintf(b, "0x%x", (a)->idVendor);                \
	ret = write(fd_gadget, b, strlen(b));             \
	close(fd_gadget);                                 \
	fd_gadget = open("idProduct", O_RDWR);            \
	sprintf(b, "0x%x", (a)->idProduct);               \
	ret = write(fd_gadget, b, strlen(b));             \
	close(fd_gadget);                                 \
	fd_gadget = open("bcdUSB", O_RDWR);               \
	sprintf(b, "0x%x", (a)->bcdUSB);                  \
	ret = write(fd_gadget, b, strlen(b));             \
	close(fd_gadget);                                 \
	fd_gadget = open("bcdDevice", O_RDWR);            \
	sprintf(b, "0x%x", (a)->bcdDevice);               \
	ret = write(fd_gadget, b, strlen(b));             \
	close(fd_gadget)

static void syz_usb_idle()
{
	sleep(1);
}

// GENERATE gadget
static int syz_usb_trans0_1(volatile long a0, volatile long a1)
{
	int fd_gadget, ret;
	char* gname = (char*)a0;
	struct configfs_attr* attrs = (struct configfs_attr*)a1;

	ret = chdir("/sys/kernel/config/usb_gadget/");
	if (ret < 0) {
		debug("chdir to usb_gadget fails..\n");
		return ret;
	}

	struct stat st;
	stat(gname, &st);
	if (S_ISDIR(st.st_mode)) {
		debug("usb_gadget/%s already exist..\n", gname);
		return -1;
	}

	/* Connection */
	ret = mkdir(gname, O_RDWR);
	if (ret < 0) {
		debug("(trans0_1) create new gadget (mkdir:%s) fails..\n", gname);
		return ret;
	}
	ret = chdir(gname);
	if (ret < 0) {
		debug("(chdir) fails..\n");
		return ret;
	}

	configfs_set_descs(attrs);
	return 0;
}

// set CONFIG
static int syz_usb_trans1_2(volatile long a0, volatile long a1)
{
	char usb_gadget[31] = "/sys/kernel/config/usb_gadget/";
	char* config = (char*)a1;
	char gpath[50];

	sprintf(gpath, "%s%s", usb_gadget, (char*)a0);

	int ret = chdir(gpath);
	if (ret < 0) {
		debug("(trans1_2) chdir to %s fails..\n", gpath);
		return ret;
	}
	ret = mkdir(config, O_RDWR);
	if (ret < 0) {
		debug("(trans1_2) mkdir (%s) fails..\n", config);
		return ret;
	}
	return 0;
}

// FUNC
static int syz_usb_trans2_3(volatile long a0, volatile long a1)
{
	char usb_gadget[31] = "/sys/kernel/config/usb_gadget/";
	char* func = (char*)a1;
	char gpath[50];

	sprintf(gpath, "%s%s", usb_gadget, (char*)a0);

	int ret = chdir(gpath);
	if (ret < 0) {
		debug("(trans2_3) chdir to %s fails..\n", gpath);
		return ret;
	}
	ret = mkdir(func, O_RDWR);
	if (ret < 0) {
		debug("(trans2_3) mkdir (%s) fails..\n", func);
		return ret;
	}
	return 0;
}

// LINK
static int syz_usb_trans3_4(volatile long a0, volatile long a1, volatile long a2)
{
	char usb_gadget[31] = "/sys/kernel/config/usb_gadget/";
	char* func = (char*)a1;
	char* config = (char*)a2;
	char gpath[50];
	int fd_gadget;

	sprintf(gpath, "%s%s", usb_gadget, (char*)a0);

	int ret = chdir(gpath);
	if (ret < 0) {
		debug("(trans3_4) chdir to %s fails..\n", gpath);
		return ret;
	}

	fd_gadget = open(".", O_RDONLY | O_DIRECTORY);
	if (fd_gadget < 0) {
		debug("(trans3_4) fail open..%d\n", fd_gadget);
		return fd_gadget;
	}
	ret = symlinkat(func, fd_gadget, config);
	if (ret < 0) {
		debug("(trans3_4) fail symlink..%s %s\n", func, config);
		return ret;
	}
	close(fd_gadget);

	debug("(trans3_4) success %s %s\n", func, config);
	return 0;
}

// BIND
static int syz_usb_trans4_5(volatile long a0, volatile long a1)
{
	char usb_gadget[31] = "/sys/kernel/config/usb_gadget/";
	char* dummy = (char*)a1;
	char gpath[50];
	int fd_gadget;

	sprintf(gpath, "%s%s", usb_gadget, (char*)a0);

	int ret = chdir(gpath);
	if (ret < 0) {
		debug("(trans4_5) chdir to %s fails..\n", gpath);
		return ret;
	}

	fd_gadget = open("UDC", O_RDWR);
	if (fd_gadget < 0) {
		debug("trans4_5 fail UDC open..%d\n", fd_gadget);
		return fd_gadget;
	}
	ret = write(fd_gadget, dummy, strlen(dummy) + 1);
	if (ret < 0) {
		debug("trans4_5 fail bind write..%d\n", ret);
		return ret;
	}
	close(fd_gadget);
	return 0;
}

static void syz_usb_trans5_4(char* g_name)
{
	int fd, ret;
	struct stat st;

	ret = chdir("/sys/kernel/config/usb_gadget/");

	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode))
		return;

	debug("trans5_4 %s", g_name);
	ret = chdir(g_name);
	if (ret < 0)
		debug("trans5_4 usb_gadget/%s fail\n", g_name);

	// unbinding gadget with dummy UDC
	fd = open("UDC", O_RDWR);
	ret = write(fd, "", 1);
	close(fd);
}

// Unlink config with function
static void syz_usb_trans4_3(char* g_name)
{
	int ret;
	struct stat st;

	ret = chdir("/sys/kernel/config/usb_gadget/");

	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode))
		return;

	debug("trans4_3 %s", g_name);
	ret = chdir(g_name);
	if (ret < 0)
		debug("trans4_3 usb_gadget/%s fail\n", g_name);

	if (!strcmp(g_name, "hid")) {
		ret = remove("configs/c.1/hid.0");
	} else if (!strcmp(g_name, "hid2")) {
		ret = remove("configs/c.2/hid.1");
	} else if (!strcmp(g_name, "prt")) {
		ret = remove("configs/c.1/printer.0");
	} else if (!strcmp(g_name, "mass")) {
		ret = remove("configs/c.1/mass_storage.0");
	} else if (!strcmp(g_name, "ncm")) {
		ret = remove("configs/c.1/ncm.0");
	} else if (!strcmp(g_name, "ecm")) {
		ret = remove("configs/c.1/ecm.0");
	} else if (!strcmp(g_name, "eem")) {
		ret = remove("configs/c.1/eem.0");
	} else if (!strcmp(g_name, "acm")) {
		ret = remove("configs/c.1/acm.0");
	} else if (!strcmp(g_name, "rndis")) {
		ret = remove("configs/c.1/rndis.0");
	} else if (!strcmp(g_name, "subset")) {
		ret = remove("configs/c.1/geth.0");
	} else if (!strcmp(g_name, "midi")) {
		ret = remove("configs/c.1/midi.0");
	} else if (!strcmp(g_name, "ss")) {
		ret = remove("configs/c.1/SourceSink.0");
	} else if (!strcmp(g_name, "serial")) {
		ret = remove("configs/c.1/gser.0");
	} else if (!strcmp(g_name, "lb")) {
		ret = remove("configs/c.1/Loopback.0");
	} else if (!strcmp(g_name, "uac1")) {
		ret = remove("configs/c.1/uac1.0");
	} else if (!strcmp(g_name, "uac2")) {
		ret = remove("configs/c.1/uac2.0");
	}
}

// remove FUNC
static void syz_usb_trans3_2(char* g_name)
{
	int ret;
	struct stat st;

	ret = chdir("/sys/kernel/config/usb_gadget/");

	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode))
		return;

	debug("trans3_2 %s\n", g_name);
	ret = chdir(g_name);
	if (ret < 0)
		debug("trans3_2 usb_gadget/%s fail\n", g_name);

	if (!strcmp(g_name, "hid")) {
		ret = rmdir("functions/hid.0");
	} else if (!strcmp(g_name, "hid2")) {
		ret = rmdir("functions/hid.1");
	} else if (!strcmp(g_name, "prt")) {
		ret = rmdir("functions/printer.0");
	} else if (!strcmp(g_name, "mass")) {
		ret = rmdir("functions/mass_storage.0");
	} else if (!strcmp(g_name, "ncm")) {
		ret = rmdir("functions/ncm.0");
	} else if (!strcmp(g_name, "ecm")) {
		ret = rmdir("functions/ecm.0");
	} else if (!strcmp(g_name, "eem")) {
		ret = rmdir("functions/eem.0");
	} else if (!strcmp(g_name, "acm")) {
		ret = rmdir("functions/acm.0");
	} else if (!strcmp(g_name, "rndis")) {
		ret = rmdir("functions/rndis.0");
	} else if (!strcmp(g_name, "subset")) {
		ret = rmdir("functions/geth.0");
	} else if (!strcmp(g_name, "midi")) {
		ret = rmdir("functions/midi.0");
	} else if (!strcmp(g_name, "ss")) {
		ret = rmdir("functions/SourceSink.0");
	} else if (!strcmp(g_name, "serial")) {
		ret = rmdir("functions/gser.0");
	} else if (!strcmp(g_name, "lb")) {
		ret = rmdir("functions/Loopback.0");
	} else if (!strcmp(g_name, "uac1")) {
		ret = rmdir("functions/uac1.0");
	} else if (!strcmp(g_name, "uac2")) {
		ret = rmdir("functions/uac2.0");
	}
}

// remove CONFIG
static void syz_usb_trans2_1(char* g_name)
{
	int ret;
	struct stat st;

	ret = chdir("/sys/kernel/config/usb_gadget/");

	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode))
		return;

	debug("trans2_1 %s", g_name);
	ret = chdir(g_name);
	if (ret < 0)
		debug("trans2_1 usb_gadget/%s fail\n", g_name);

	ret = rmdir("configs/c.1");
	if (ret < 0)
		debug("trans2_1 rmdir /%s fail\n", g_name);
}

// remove gadget
static void syz_usb_trans1_0(char* g_name)
{
	int ret;

	ret = chdir("/sys/kernel/config/usb_gadget/");
	if (ret < 0) {
		debug("chdir to usb_gadget fails..\n");
		return;
	}

	struct stat st;
	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode)) {
		debug("usb_gadget/%s NOT exist..\n", g_name);
		return;
	}

	debug("trans1_0 %s", g_name);
	ret = rmdir(g_name);
	if (ret < 0) {
		debug("remove gadget (rmdir:%s) fails..\n", g_name);
		return;
	}

	return;
}

static void syz_usb_configfs_hid(volatile long a0)
{
	int fd_gadget, ret;
	struct configfs_attr* attrs = (struct configfs_attr*)a0;

	ret = chdir("/sys/kernel/config/usb_gadget/");

	struct stat st;
	stat("hid", &st);
	if (S_ISDIR(st.st_mode)) {
		debug("usb_gadget/hid already exist..\n");
		return;
	}
	//debug("dir no exist..\n");

	/* Connection */
	ret = mkdir("hid", O_RDWR);
	ret = chdir("hid");

	// 2. set DEVICE DESC
	configfs_set_descs(attrs);

	// 3. set CONFIG DESC "keyboard"
	ret = mkdir("configs/c.1", O_RDWR);
	// 4. set functions
	ret = mkdir("functions/hid.0", O_RDWR);
	// 4-1.
	fd_gadget = open("functions/hid.0/subclass", O_RDWR);
	ret = write(fd_gadget, "1", 2);
	close(fd_gadget);
	fd_gadget = open("functions/hid.0/protocol", O_RDWR);
	ret = write(fd_gadget, "1", 2);
	close(fd_gadget);
	fd_gadget = open("functions/hid.0/report_length", O_RDWR);
	ret = write(fd_gadget, "8", 2);
	close(fd_gadget);
	fd_gadget = open("functions/hid.0/report_desc", O_RDWR);
	ret = write(fd_gadget, report_desc_keyboard, sizeof(report_desc_keyboard));
	close(fd_gadget);

	// 5. link config with function
	fd_gadget = open(".", O_RDONLY | O_DIRECTORY);
	//  if (ret < 0)
	//    debug("fail open..%d\n", ret);
	ret = symlinkat("functions/hid.0", fd_gadget, "configs/c.1/hid.0");
	if (ret < 0)
		debug("syz_usb_configfs_hid fail symlink..%d\n", ret);
	close(fd_gadget);

	// 6. bind with dummy UDC
	fd_gadget = open("UDC", O_RDWR);
	ret = write(fd_gadget, "dummy_udc.0", 12);
	close(fd_gadget);

	//  int f = open("/sys/kernel/config/usb_gadget/hid", O_RDONLY);
	//  if (f > 0) {
	//    //char cwd[1024];
	//    //int ret = read(f, cwd, 5);
	//	  //debug("syz_usb_hid 2 read: %c%c%c%c %d fd:%d\n", cwd[0], cwd[1], cwd[2], cwd[3], ret, f);
	//    close(f);
	//  } else debug("syz_usb_hid 2 read fail\n");
}

static void syz_usb_configfs_hid2(volatile long a0)
{
	int fd_gadget, ret;
	struct configfs_attr* attrs = (struct configfs_attr*)a0;

	ret = chdir("/sys/kernel/config/usb_gadget/");

	struct stat st;
	stat("hid2", &st);
	if (S_ISDIR(st.st_mode)) {
		debug("usb_gadget/hid2 already exist..\n");
		return;
	}

	/* Connection */
	ret = mkdir("hid2", O_RDWR);
	ret = chdir("hid2");

	// 2. set DEVICE DESC
	configfs_set_descs(attrs);

	// 3. set CONFIG DESC "keyboard"
	ret = mkdir("configs/c.2", O_RDWR);
	// 4. set functions
	ret = mkdir("functions/hid.1", O_RDWR);
	fd_gadget = open("functions/hid.1/subclass", O_RDWR);
	ret = write(fd_gadget, "1", 2);
	close(fd_gadget);
	fd_gadget = open("functions/hid.1/protocol", O_RDWR);
	ret = write(fd_gadget, "1", 2);
	close(fd_gadget);
	fd_gadget = open("functions/hid.1/report_length", O_RDWR);
	ret = write(fd_gadget, "8", 2);
	close(fd_gadget);
	fd_gadget = open("functions/hid.1/report_desc", O_RDWR);
	ret = write(fd_gadget, report_desc_keyboard, sizeof(report_desc_keyboard));
	close(fd_gadget);

	// 5. link config with function
	fd_gadget = open(".", O_RDONLY | O_DIRECTORY);
	ret = symlinkat("functions/hid.1", fd_gadget, "configs/c.2/hid.1");
	if (ret < 0)
		debug("syz_usb_configfs_hid2 fail symlink..%d\n", ret);
	close(fd_gadget);

	// 6. bind with dummy UDC
	fd_gadget = open("UDC", O_RDWR);
	ret = write(fd_gadget, "dummy_udc.1", 12);
	close(fd_gadget);
}

























#if USB_DEBUG
#include <linux/hid.h>
#include <linux/usb/cdc.h>
#include <linux/usb/ch11.h>
#include <linux/usb/ch9.h>

static void analyze_control_request(struct usb_ctrlrequest* ctrl, bool setup)
{
	switch (ctrl->bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_STANDARD:
		switch (ctrl->bRequest) {
		case USB_REQ_GET_DESCRIPTOR:
			switch (ctrl->wValue >> 8) {
			case USB_DT_DEVICE:
				debug("analyze_control_request: USB_DT_DEVICE (0x%x, 0x%x, 0x%x) %d\n",
				      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
				return;
			case USB_DT_CONFIG:
				debug("analyze_control_request: USB_DT_CONFIG (0x%x, 0x%x, 0x%x) %d\n",
				      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
				return;
			case USB_DT_STRING:
				debug("analyze_control_request: USB_DT_STRING (0x%x, 0x%x, 0x%x) %d\n",
				      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
				return;
			case HID_DT_REPORT:
				debug("analyze_control_request: HID_DT_REPORT (0x%x, 0x%x, 0x%x) %d\n",
				      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
				return;
			case USB_DT_BOS:
				debug("analyze_control_request: USB_DT_BOS (0x%x, 0x%x, 0x%x) %d\n",
				      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
				return;
			case USB_DT_HUB:
				debug("analyze_control_request: USB_DT_HUB (0x%x, 0x%x, 0x%x) %d\n",
				      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
				return;
			case USB_DT_SS_HUB:
				return;
			}
		}
		break;
	case USB_TYPE_CLASS:
		switch (ctrl->bRequest) {
		case USB_REQ_GET_INTERFACE:
			debug("analyze_control_request: USB_REQ_GET_INTERFACE (0x%x, 0x%x, 0x%x) %d\n",
			      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
			return;
		case USB_REQ_GET_CONFIGURATION:
			debug("analyze_control_request: USB_REQ_GET_CONFIGURATION (0x%x, 0x%x, 0x%x) %d\n",
			      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
			return;
		case USB_REQ_GET_STATUS:
			debug("analyze_control_request: USB_REQ_GET_STATUS (0x%x, 0x%x, 0x%x) %d\n",
			      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
			return;
		case USB_CDC_GET_NTB_PARAMETERS:
			debug("analyze_control_request: USB_CDC_GET_NTB_PARAMETERS (0x%x, 0x%x, 0x%x) %d\n",
			      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, setup);
			return;
		}
	}
	fail("analyze_control_request: unknown control request (0x%x, 0x%x, 0x%x)",
	     ctrl->bRequestType, ctrl->bRequest, ctrl->wValue);
}
#endif

static volatile long syz_usb_connect(volatile long a0, volatile long a1, volatile long a2, volatile long a3)
{
	uint64 speed = a0;
	uint64 dev_len = a1;
	char* dev = (char*)a2;
	struct vusb_connect_descriptors* descs = (struct vusb_connect_descriptors*)a3;

	debug("syz_usb_connect: dev: %p\n", dev);
	if (!dev) {
		debug("syz_usb_connect: dev is null\n");
		return -1;
	}

	debug("syz_usb_connect: device data:\n");
	debug_dump_data(dev, dev_len);

	int fd = usb_raw_open();
	if (fd < 0) {
		debug("syz_usb_connect: usb_raw_open failed with %d\n", fd);
		return fd;
	}
	if (fd >= MAX_FDS) {
		close(fd);
		debug("syz_usb_connect: too many open fds\n");
		return -1;
	}
	debug("syz_usb_connect: usb_raw_open success\n");

	struct usb_device_index* index = add_usb_index(fd, dev, dev_len);
	if (!index) {
		debug("syz_usb_connect: add_usb_index failed\n");
		return -1;
	}
	debug("syz_usb_connect: add_usb_index success\n");

	// TODO: consider creating two dummy_udc's per proc to increace the chance of
	// triggering interaction between multiple USB devices within the same program.
	char device[32];
	sprintf(&device[0], "dummy_udc.%llu", procid);
	int rv = usb_raw_init(fd, speed, "dummy_udc", &device[0]);
	if (rv < 0) {
		debug("syz_usb_connect: usb_raw_init failed with %d\n", rv);
		return rv;
	}
	debug("syz_usb_connect: usb_raw_init success\n");

	rv = usb_raw_run(fd);
	if (rv < 0) {
		debug("syz_usb_connect: usb_raw_run failed with %d\n", rv);
		return rv;
	}
	debug("syz_usb_connect: usb_raw_run success\n");

	bool done = false;
	while (!done) {
		struct usb_raw_control_event event;
		event.inner.type = 0;
		event.inner.length = sizeof(event.ctrl);
		rv = usb_raw_event_fetch(fd, (struct usb_raw_event*)&event);
		if (rv < 0) {
			debug("syz_usb_connect: usb_raw_event_fetch failed with %d\n", rv);
			return rv;
		}
		if (event.inner.type != USB_RAW_EVENT_CONTROL)
			continue;

		debug("syz_usb_connect: bReqType: 0x%x (%s), bReq: 0x%x, wVal: 0x%x, wIdx: 0x%x, wLen: %d\n",
		      event.ctrl.bRequestType, (event.ctrl.bRequestType & USB_DIR_IN) ? "IN" : "OUT",
		      event.ctrl.bRequest, event.ctrl.wValue, event.ctrl.wIndex, event.ctrl.wLength);

		analyze_control_request(&event.ctrl, 1);

		bool response_found = false;
		char* response_data = NULL;
		uint32 response_length = 0;

		if (event.ctrl.bRequestType & USB_DIR_IN) {
			NONFAILING(response_found = lookup_connect_response(fd, descs, &event.ctrl, &response_data, &response_length));
			if (!response_found) {
				debug("syz_usb_connect: unknown control IN request\n");
				return -1;
			}
		} else {
			if ((event.ctrl.bRequestType & USB_TYPE_MASK) != USB_TYPE_STANDARD ||
			    event.ctrl.bRequest != USB_REQ_SET_CONFIGURATION) {
				fail("syz_usb_connect: unknown control OUT request");
				return -1;
			}
			done = true;
		}

		if (done) {
			rv = configure_device(fd);
			if (rv < 0) {
				debug("syz_usb_connect: configure_device failed with %d\n", rv);
				return rv;
			}
		}

		struct usb_raw_ep_io_data response;
		response.inner.ep = 0;
		response.inner.flags = 0;
		if (response_length > sizeof(response.data))
			response_length = 0;
		if (event.ctrl.wLength < response_length)
			response_length = event.ctrl.wLength;
		response.inner.length = response_length;
		if (response_data)
			memcpy(&response.data[0], response_data, response_length);
		else
			memset(&response.data[0], 0, response_length);

		if (event.ctrl.bRequestType & USB_DIR_IN) {
			debug("syz_usb_connect: writing %d bytes\n", response.inner.length);
			rv = usb_raw_ep0_write(fd, (struct usb_raw_ep_io*)&response);
		} else {
			rv = usb_raw_ep0_read(fd, (struct usb_raw_ep_io*)&response);
			debug("syz_usb_connect: read %d bytes\n", response.inner.length);
			debug_dump_data(&event.data[0], response.inner.length);
		}
		if (rv < 0) {
			debug("syz_usb_connect: usb_raw_ep0_read/write failed with %d\n", rv);
			return rv;
		}
	}

	sleep_ms(200);

	debug("syz_usb_connect: configured\n");

	return fd;
}

#if SYZ_EXECUTOR || __NR_syz_usb_control_io
struct vusb_descriptor {
	uint8 req_type;
	uint8 desc_type;
	uint32 len;
	char data[0];
} __attribute__((packed));

struct vusb_descriptors {
	uint32 len;
	struct vusb_descriptor* generic;
	struct vusb_descriptor* descs[0];
} __attribute__((packed));

struct vusb_response {
	uint8 type;
	uint8 req;
	uint32 len;
	char data[0];
} __attribute__((packed));

struct vusb_responses {
	uint32 len;
	struct vusb_response* generic;
	struct vusb_response* resps[0];
} __attribute__((packed));

static bool lookup_control_response(struct vusb_descriptors* descs, struct vusb_responses* resps,
				    struct usb_ctrlrequest* ctrl, char** response_data, uint32* response_length)
{
	int descs_num = 0;
	int resps_num = 0;

	if (descs)
		descs_num = (descs->len - offsetof(struct vusb_descriptors, descs)) / sizeof(descs->descs[0]);
	if (resps)
		resps_num = (resps->len - offsetof(struct vusb_responses, resps)) / sizeof(resps->resps[0]);

	uint8 req = ctrl->bRequest;
	uint8 req_type = ctrl->bRequestType & USB_TYPE_MASK;
	uint8 desc_type = ctrl->wValue >> 8;

	if (req == USB_REQ_GET_DESCRIPTOR) {
		int i;

		for (i = 0; i < descs_num; i++) {
			struct vusb_descriptor* desc = descs->descs[i];
			if (!desc)
				continue;
			if (desc->req_type == req_type && desc->desc_type == desc_type) {
				*response_length = desc->len;
				if (*response_length != 0)
					*response_data = &desc->data[0];
				else
					*response_data = NULL;
				return true;
			}
		}

		if (descs && descs->generic) {
			*response_data = &descs->generic->data[0];
			*response_length = descs->generic->len;
			return true;
		}
	} else {
		int i;

		for (i = 0; i < resps_num; i++) {
			struct vusb_response* resp = resps->resps[i];
			if (!resp)
				continue;
			if (resp->type == req_type && resp->req == req) {
				*response_length = resp->len;
				if (*response_length != 0)
					*response_data = &resp->data[0];
				else
					*response_data = NULL;
				return true;
			}
		}

		if (resps && resps->generic) {
			*response_data = &resps->generic->data[0];
			*response_length = resps->generic->len;
			return true;
		}
	}

	return false;
}

static volatile long syz_usb_control_io(volatile long a0, volatile long a1, volatile long a2)
{
	int fd = a0;
	struct vusb_descriptors* descs = (struct vusb_descriptors*)a1;
	struct vusb_responses* resps = (struct vusb_responses*)a2;

	struct usb_raw_control_event event;
	event.inner.type = 0;
	event.inner.length = USB_MAX_PACKET_SIZE;
	int rv = usb_raw_event_fetch(fd, (struct usb_raw_event*)&event);
	if (rv < 0) {
		debug("syz_usb_control_io: usb_raw_ep0_read failed with %d\n", rv);
		return rv;
	}
	if (event.inner.type != USB_RAW_EVENT_CONTROL) {
		debug("syz_usb_control_io: wrong event type: %d\n", (int)event.inner.type);
		return -1;
	}

	debug("syz_usb_control_io: bReqType: 0x%x (%s), bReq: 0x%x, wVal: 0x%x, wIdx: 0x%x, wLen: %d\n",
	      event.ctrl.bRequestType, (event.ctrl.bRequestType & USB_DIR_IN) ? "IN" : "OUT",
	      event.ctrl.bRequest, event.ctrl.wValue, event.ctrl.wIndex, event.ctrl.wLength);

	bool response_found = false;
	char* response_data = NULL;
	uint32 response_length = 0;

	if ((event.ctrl.bRequestType & USB_DIR_IN) && event.ctrl.wLength) {
		NONFAILING(response_found = lookup_control_response(descs, resps, &event.ctrl, &response_data, &response_length));
		if (!response_found) {
#if USB_DEBUG
			analyze_control_request(&event.ctrl, 0);
#endif
			debug("syz_usb_control_io: unknown control IN request\n");
			return -1;
		}
	} else {
		if ((event.ctrl.bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD ||
		    event.ctrl.bRequest == USB_REQ_SET_INTERFACE) {
			int iface_num = event.ctrl.wIndex;
			int alt_set = event.ctrl.wValue;
			debug("syz_usb_control_io: setting interface (%d, %d)\n", iface_num, alt_set);
			int iface_index = lookup_interface(fd, iface_num, alt_set);
			if (iface_index < 0) {
				debug("syz_usb_control_io: interface (%d, %d) not found\n", iface_num, alt_set);
			} else {
				set_interface(fd, iface_index);
				debug("syz_usb_control_io: interface (%d, %d) set\n", iface_num, alt_set);
			}
		}

		response_length = event.ctrl.wLength;
	}

	struct usb_raw_ep_io_data response;
	response.inner.ep = 0;
	response.inner.flags = 0;
	if (response_length > sizeof(response.data))
		response_length = 0;
	if (event.ctrl.wLength < response_length)
		response_length = event.ctrl.wLength;
	if ((event.ctrl.bRequestType & USB_DIR_IN) && !event.ctrl.wLength) {
		// Something fishy is going on, try to read more data.
		response_length = USB_MAX_PACKET_SIZE;
	}
	response.inner.length = response_length;
	if (response_data)
		memcpy(&response.data[0], response_data, response_length);
	else
		memset(&response.data[0], 0, response_length);

	if ((event.ctrl.bRequestType & USB_DIR_IN) && event.ctrl.wLength) {
		debug("syz_usb_control_io: writing %d bytes\n", response.inner.length);
		debug_dump_data(&response.data[0], response.inner.length);
		rv = usb_raw_ep0_write(fd, (struct usb_raw_ep_io*)&response);
	} else {
		rv = usb_raw_ep0_read(fd, (struct usb_raw_ep_io*)&response);
		debug("syz_usb_control_io: read %d bytes\n", response.inner.length);
		debug_dump_data(&response.data[0], response.inner.length);
	}
	if (rv < 0) {
		debug("syz_usb_control_io: usb_raw_ep0_read/write failed with %d\n", rv);
		return rv;
	}

	sleep_ms(200);

	return 0;
}
#endif

#if SYZ_EXECUTOR || __NR_syz_usb_ep_write
static volatile long syz_usb_ep_write(volatile long a0, volatile long a1, volatile long a2, volatile long a3)
{
	int fd = a0;
	uint16 ep = a1;
	uint32 len = a2;
	char* data = (char*)a3;

	struct usb_raw_ep_io_data io_data;
	io_data.inner.ep = ep;
	io_data.inner.flags = 0;
	if (len > sizeof(io_data.data))
		len = sizeof(io_data.data);
	io_data.inner.length = len;
	NONFAILING(memcpy(&io_data.data[0], data, len));

	int rv = usb_raw_ep_write(fd, (struct usb_raw_ep_io*)&io_data);
	if (rv < 0) {
		debug("syz_usb_ep_write: usb_raw_ep_write failed with %d\n", rv);
		return rv;
	}

	sleep_ms(200);

	return 0;
}
#endif

#if SYZ_EXECUTOR || __NR_syz_usb_ep_read
static volatile long syz_usb_ep_read(volatile long a0, volatile long a1, volatile long a2, volatile long a3)
{
	int fd = a0;
	uint16 ep = a1;
	uint32 len = a2;
	char* data = (char*)a3;

	struct usb_raw_ep_io_data io_data;
	io_data.inner.ep = ep;
	io_data.inner.flags = 0;
	if (len > sizeof(io_data.data))
		len = sizeof(io_data.data);
	io_data.inner.length = len;

	int rv = usb_raw_ep_read(fd, (struct usb_raw_ep_io*)&io_data);
	if (rv < 0) {
		debug("syz_usb_ep_read: usb_raw_ep_read failed with %d\n", rv);
		return rv;
	}

	NONFAILING(memcpy(&data[0], &io_data.data[0], io_data.inner.length));

	debug("syz_usb_ep_read: received data:\n");
	debug_dump_data(&io_data.data[0], io_data.inner.length);

	sleep_ms(200);

	return 0;
}
#endif

#if SYZ_EXECUTOR || __NR_syz_usb_disconnect
static volatile long syz_usb_disconnect(volatile long a0)
{
	int fd = a0;

	int rv = close(fd);

	sleep_ms(200);

	return rv;
}
#endif
