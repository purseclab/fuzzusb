
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


/*-------------------------------------------------------------------------*/

// FIXME make these public somewhere; usbdevfs.h?

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
	}

	fflush (stdout);
	
	if (dev->forever)
		goto restart;

	close (fd);
	return arg;
}

static void setup_test (struct usbtest_param *param, unsigned type, unsigned dir_out, char *hex_str)
{
	param->is_fuzzusb = 1;
	param->is_ctrl = 0;
	param->is_int = 0;
	param->is_bulk = 0;
	param->is_iso = 0;
	param->is_set_alt = 0;

	param->is_dir_out = dir_out;

  if (type == 1)
	  param->is_int = 1;
  else if (type == 2)
	  param->is_bulk = 1;
  else
	  param->is_iso = 1;

  if (hex_str != NULL && strlen(hex_str) > 0) {
	  param->data_size = strlen(hex_str);
	  param->data = hex_str;
  }
  else {
	  param->data_size = 0;
	  param->data = NULL;
  }
}

int main (int argc, char **argv)
{
	int			c;
	struct testdev		*entry;
	char			*device;
	int			forever = 0;
	int			test = -1 /* all */;
	struct usbtest_param	param;
	int ret = 0;

  unsigned iter; // iteration
  unsigned len; // length
  unsigned type; // 1:int 2:bulk 3:iso
  unsigned dir_out; // 1:out 2:in
	char *hex_str = NULL;

	unsigned long *cover, n, i;
  int fd;

	device = getenv ("DEVICE");

	while ((c = getopt (argc, argv, "D:h:t:i:l:d:a:")) != EOF)
	  switch (c) {
	  case 'D':	/* device, if only one */
		  device = optarg;
		  continue;
	  case 't': // data transfer type
		  type = atoi(optarg);
		  continue;
	  case 'i': // 
		  iter = atoi(optarg);
	    printf("-i %d\n", iter);
		  continue;
	  case 'l': // 
		  len = atoi(optarg);
	    printf("-l %d\n", iter);
		  continue;
	  case 'd': // direction
		  dir_out = atoi(optarg);
		  continue;
	  case 'a':
		  hex_str = optarg;
      if (!strlen(hex_str))
        hex_str = NULL;
		  continue;
	  case 'h':
	  default:
  usage:
		  fprintf (stderr, "usage: %s\t-D dev		only test specific device\n", argv[0]);
		  return 1;
	  }

	printf("%s %d %d %d %d here\n", argv[0], optind, argc, type, dir_out);


	if (optind != argc)
		goto usage;

	if (!device) {
		fprintf (stderr, "must specify '-D dev', or DEVICE=/dev/bus/usb/BBB/DDD in env\n");
		goto usage;
	}

	param.iterations = iter;
	param.length = len;
	param.vary = 1024;
	param.sglen = 32;

  setup_test (&param, type, dir_out, hex_str);

	printf("test device : %s, type: %d, dir_out: %d\n", device, type, dir_out);

	if (!device) {
		fprintf (stderr, "must specify '-D dev', or DEVICE=/dev/bus/usb/BBB/DDD in env\n");
    return 0;
  }

	struct testdev dev;
	memset (&dev, 0, sizeof dev);
	dev.name = device;
	dev.param = param;
	dev.ifnum = 0; // ??
	dev.forever = forever;

	ret = handle_testdev (&dev) != &dev;

	return ret;
}
