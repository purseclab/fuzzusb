
#include <stdio.h>
#include <string.h>
#include <ftw.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

#include <byteswap.h>

#include "fuzzusb.h"


// FIXME make these public somewhere; usbdevfs.h?

/*-------------------------------------------------------------------------*/

static char *speed (enum usb_device_speed s)
{
	switch (s) {
	case USB_SPEED_UNKNOWN:	return "unknown";
	case USB_SPEED_LOW:	return "low";
	case USB_SPEED_FULL:	return "full";
	case USB_SPEED_HIGH:	return "high";
	default:		return "??";
	}
}

struct testdev {
	struct testdev		*next;
	char			*name;
	pthread_t		thread;
	enum usb_device_speed	speed;
	unsigned		ifnum : 8; // 0 or 1 ??
	unsigned		forever : 1;

	struct usbtest_param	param;
};
static struct testdev		*testdevs;

static int testdev_ffs_ifnum(FILE *fd)
{
	union {
		char buf[255];
		struct usb_interface_descriptor intf;
	} u;

	for (;;) {
		if (fread(u.buf, 1, 1, fd) != 1)
			return -1;
		if (fread(u.buf + 1, (unsigned char)u.buf[0] - 1, 1, fd) != 1)
			return -1;

		if (u.intf.bLength == sizeof u.intf
		 && u.intf.bDescriptorType == USB_DT_INTERFACE
		 && u.intf.bNumEndpoints == 2
		 && u.intf.bInterfaceClass == USB_CLASS_VENDOR_SPEC
		 && u.intf.bInterfaceSubClass == 0
		 && u.intf.bInterfaceProtocol == 0)
			return (unsigned char)u.intf.bInterfaceNumber;
	}
}

static int usbdev_ioctl (int fd, int ifno, unsigned request, void *param)
{
	struct usbdevfs_ioctl	wrapper;

	wrapper.ifno = ifno;
	wrapper.ioctl_code = request;
	wrapper.data = param;

  int fd2, ret;
	unsigned long *cover;
  cover = fuzzusb_kcov_init(&fd2);

	ret = ioctl (fd, USBDEVFS_IOCTL, &wrapper);

  if (cover != NULL)
    fuzzusb_kcov_end(fd2, cover);

	return ret;
}

static void *handle_testdev (void *arg)
{
	struct testdev		*dev = arg;
	int			fd, i;
	int			status;

	if ((fd = open (dev->name, O_RDWR)) < 0) {
		perror ("can't open dev file r/w");
		return 0;
	}

restart:
	status = usbdev_ioctl (fd, dev->ifnum, USBTEST_REQUEST, 
              &dev->param);

	/* FIXME need a "syslog it" option for background testing */

	/* NOTE: each thread emits complete lines; no fragments! */
	if (status < 0) {
		char	buf [80];
		int	err = errno;

		if (strerror_r (errno, buf, sizeof buf)) {
			snprintf (buf, sizeof buf, "error %d", err);
			errno = err;
		}
		printf ("%s test %d --> %d (%s)\n", dev->name, i, errno, buf);
	}

	fflush (stdout);
	
	if (dev->forever)
		goto restart;

	close (fd);
	return arg;
}

static void setup_test (struct usbtest_param	*param, int test_val, int arg1, int arg2)
{
	param->is_fuzzusb = 1;
	param->is_ctrl = 1;
	param->is_bulk = 0;
	param->is_int = 0;
	param->is_set_alt = 0;

  param->ctrl_out.bRequestType = USB_DIR_IN|USB_RECIP_DEVICE;
	param->ctrl_out.bRequest = USB_REQ_GET_DESCRIPTOR;

  if (!test_val) { // pure random gen
    srand(time(NULL));
    param->ctrl_out.bRequestType = rand() % UCHAR_MAX;
	  param->ctrl_out.bRequest = rand() % UCHAR_MAX;
	  param->ctrl_out.wValue = rand() % USHRT_MAX;
  	param->ctrl_out.wIndex = rand() % USHRT_MAX;
	  param->ctrl_out.wLength = rand() % USHRT_MAX;
  } else if (test_val == 1) { // set intf
	  param->is_ctrl = 0;
	  param->is_set_alt = 1;
	  param->iterations = arg1; // intf
	  param->length = arg2; // alt
  } else if (test_val == 2) { // get dev desc
	  param->ctrl_out.wValue = __bswap_16(USB_DT_DEVICE << 8);
	  param->ctrl_out.wLength = __bswap_16(sizeof(struct usb_device_descriptor));
  } else if (test_val == 3) { // get first config desc
	  param->ctrl_out.wValue = __bswap_16(USB_DT_CONFIG << 8);
	  param->ctrl_out.wLength = __bswap_16(sizeof(struct usb_config_descriptor));
  } else if (test_val == 4) { // get altset (OFTEN STALLS)
    param->ctrl_out.bRequestType = USB_DIR_IN|USB_RECIP_INTERFACE;
	  param->ctrl_out.bRequest = USB_REQ_GET_INTERFACE;
	  param->ctrl_out.wLength = __bswap_16(1);
  } else if (test_val == 5) { // get intf status
    param->ctrl_out.bRequestType = USB_DIR_IN|USB_RECIP_INTERFACE;
	  param->ctrl_out.bRequest = USB_REQ_GET_STATUS;
	  param->ctrl_out.wLength = __bswap_16(2);
  } else if (test_val == 6) { // get dev status
	  param->ctrl_out.bRequest = USB_REQ_GET_STATUS;
	  param->ctrl_out.wLength = __bswap_16(2);
  } else if (test_val == 7) { // get dev qual
    param->ctrl_out.bRequestType = USB_DIR_IN|USB_RECIP_INTERFACE;
		param->ctrl_out.wValue = __bswap_16(USB_DT_DEVICE_QUALIFIER << 8);
	  param->ctrl_out.wLength = __bswap_16(sizeof(struct usb_qualifier_descriptor));
  } else if (test_val == 8) { // get first config desc
    param->ctrl_out.bRequestType = USB_DIR_IN|USB_RECIP_INTERFACE;
		param->ctrl_out.wValue = __bswap_16(USB_DT_CONFIG << 8|0);
	  param->ctrl_out.wLength = __bswap_16(sizeof(struct usb_config_descriptor) + sizeof(struct usb_interface_descriptor));
  }
}

int main (int argc, char **argv)
{
	int			c;
	struct testdev		*entry;
	char			*device;
	int			forever = 0;
	struct usbtest_param	param;
	int	test_case = -1 /* all */;
	int	arg1, arg2;
	int ret = 0;

	unsigned long *cover, n, i;
  int fd;

	device = getenv ("DEVICE");

	while ((c = getopt (argc, argv, "D:c:a:b:h:")) != EOF)
	  switch (c) {
	  case 'D':	/* device, if only one */
		  device = optarg;
		  continue;
	  case 'c':	
		  test_case = atoi(optarg);
	    printf("-c %d\n", test_case);
		  continue;
	  case 'a':	
		  arg1 = atoi(optarg);
	    printf("-a %d\n", arg1);
		  continue;
	  case 'b':	
		  arg2 = atoi(optarg);
	    printf("-b %d\n", arg2);
		  continue;
	  case 'h':
	  default:
  usage:
		  fprintf (stderr, "usage: %s\t-D dev	only test specific device\n", argv[0]);
		  return 1;
	  }

	printf("test device : %s, test_case:%d\n", device, test_case);


	if (optind != argc)
		goto usage;

	if (!device) {
		fprintf (stderr, "must specify '-D dev', or DEVICE=/dev/bus/usb/BBB/DDD in env\n");
		goto usage;
	}

	param.iterations = 1000;
	param.length = 1024;
	param.vary = 1024;
	param.sglen = 32;

	struct testdev dev;
	memset (&dev, 0, sizeof dev);
	dev.name = device;
	dev.ifnum = 0; // ??
	dev.forever = forever;

  setup_test(&param, test_case, arg1, arg2);
	dev.param = param;
	ret = handle_testdev (&dev) != &dev;

	return ret;
}
