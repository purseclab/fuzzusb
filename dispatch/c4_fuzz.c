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

#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <sys/poll.h>

#include "fuzzusb.h"

// FIXME make these public somewhere; usbdevfs.h?


static void get_usb_interface(char *path, char *res)
{
	int ret, fd;

  // path : "/sys/kernel/config/usb_gadget/eem/functions/eem.0/ifname"
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("%s open fail\n", path);
		return ;
	}
	ret = read(fd, res, 5);
  if (ret < 0) { 
		printf("%s read fail\n", path);
    return;
  }

  res[ret-1] = '\0';
	close(fd);
}

static void ifconfig_setup(char* iface, const char* ip)
{
	int fd, ret;
	struct ifreq ifr;
	struct sockaddr_in* addr;

	printf("ipconfig %s %s up\n", iface, ip);

	/*AF_INET - to define network interface IPv4*/
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	/*AF_INET - to define IPv4 Address type.*/
	ifr.ifr_addr.sa_family = AF_INET;

	/*eth0 - define the ifr_name - port name
    where network attached.*/
	memcpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

	/*defining the sockaddr_in*/
	addr = (struct sockaddr_in*)&ifr.ifr_addr;

	/*convert ip address in correct format to write*/
	inet_pton(AF_INET, ip, &addr->sin_addr);

	/*Setting the Ip Address using ioctl*/
	if (ioctl(fd, SIOCSIFADDR, &ifr))
		perror("ioctl fail");
  else {
	  close(fd);
	  printf("IP Address updated sucessfully.\n");
  }
}

static void c4_fuzz(char *syscall, char *arg, char *arg1)
{
  int fd, size;
  int ret;
  char buf[10];

  fd = 3; // for test

  if (!strcmp(syscall, "write")) { // syscall:"write", arg:"/dev/hidg0", arg1:"hello"
    printf ("write %s:%s:%s ", syscall, arg, arg1);
    fd = open(arg, O_RDWR);
    if (fd < 0) {
      printf ("open fails\n"); return;
    }
    ret = write(fd, arg1, sizeof(arg1));
    printf ("ret:%d\n", ret);
    close(fd);
  }
  else if (!strcmp(syscall, "read")) { // syscall:"read", arg:"/dev/hidg0", arg1:size
    printf ("read %s:%s:%s ", syscall, arg, arg1);
    size = atoi(arg1);
    fd = open(arg, O_RDWR);
    if (fd < 0) {
      printf ("open fails\n"); return;
    }
    ret = read(fd, buf, size);
    printf ("ret:%d\n", ret);
    close(fd);
  }
  else if (!strcmp(syscall, "ioctl")) { // syscall:"ioctl", arg:"/dev/hidg0", arg1:cmd
    printf ("ioctl %s:%s:%s ", syscall, arg, arg1);
    unsigned cmd = atoi(arg1);
    fd = open(arg, O_RDWR);
    if (fd < 0) {
      printf ("open fails\n"); return;
    }
    ret = ioctl(fd, cmd, NULL);
    printf ("ret:%d\n", ret);
    close(fd);
  }
  else if (!strcmp(syscall, "poll")) { // syscall:"poll", arg:"/dev/hidg0", arg1:size
    printf ("poll %s:%s:%s ", syscall, arg, arg1);
	  struct pollfd Events[2];

    fd = open(arg, O_RDWR);
    int fd1 = open(arg, O_RDWR);
    if (fd < 0) {
      printf ("open fails\n"); return;
    }

	  memset (Events, 0, sizeof(Events));
	  Events[0].fd = fd;
	  Events[0].events = POLLIN | POLLERR;
	  Events[1].fd = fd1;
	  Events[1].events = POLLOUT;

		ret = poll( (struct pollfd *)&Events, 2, 5000);
		if(ret < 0 )
			printf("poll err\n");
		else if( 0 == ret)
			printf("No event!!\n");
		else
		{
			if( POLLERR & Events[0].revents )
				printf("error\n");
			else if( POLLIN & Events[0].revents )
			{
				printf("POLLIN\n");
			}
			else if (POLLOUT & Events[1].revents )
			{
				printf("POLLOUT\n");
			}
		}
    close(fd);
	  close(fd1);
  }
  else if (!strcmp(syscall, "fsync")) { // syscall:"fsync", arg:"/dev/hidg0"
    printf ("fsync %s:%s ", syscall, arg);
    fd = open(arg, O_RDWR);
    if (fd < 0) {
      printf ("open fails\n"); return;
    }
    ret = fsync(fd);
    printf ("ret:%d\n", ret);
    close(fd);
  }
  else if (!strcmp(syscall, "open")) {
    printf ("%s:%s ", syscall, arg);
    fd = open(arg, O_RDWR);
    if (fd < 0) {
      printf ("open fails\n"); return;
    }
    printf ("open %s:%s\n", syscall, arg);
  }
  else if (!strcmp(syscall, "close")) {
    printf ("%s\n", syscall );
    close(fd);
  }
  else if (!strcmp(syscall, "ifconfig")) { // arg:"", arg1:"192.168.1.6"
    char res_buf[5];
    printf ("%s:%s:%s", syscall, arg, arg1);
    get_usb_interface(arg, res_buf);
    printf ("usb_ifce: %s\n", res_buf);
	  ifconfig_setup(res_buf, arg1);
  }
}

int main (int argc, char **argv)
{
	int			c;
	char			*syscall;
	int			forever = 0;
  char			*arg, *arg1;

	while ((c = getopt (argc, argv, "s:h:a:b:")) != EOF)
	  switch (c) {
	  case 's':	
		  syscall = optarg;
		  continue;
	  case 'a':
		  arg = optarg;
		  continue;
	  case 'b':
		  arg1 = optarg;
		  continue;
	  case 'h':
	  default:
  usage:
		  fprintf (stderr, "usage: %s\t-s syscall\n", argv[0]);
		  return 1;
	  }

	if (optind != argc)
		goto usage;


  int fd2, ret;
	unsigned long *cover;
  cover = fuzzusb_kcov_init(&fd2);

  c4_fuzz(syscall, arg, arg1);

  if (cover != NULL)
    fuzzusb_kcov_end(fd2, cover);

	return 0;
}
