
#include <stdio.h>
#include <string.h>
#include <ftw.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>

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

#include "fuzzusb.h"

// FIXME make these public somewhere; usbdevfs.h?

char *g_path = "/sys/kernel/config/usb_gadget/";


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

static int trans0_1(char* gname, char *idvendor, char *idproduct, char *devclass)
{
	int fd_gadget, ret;

	ret = chdir(g_path);
	if (ret < 0) {
		printf("chdir to usb_gadget fails..\n");
		return ret;
	}

	struct stat st;
	stat(gname, &st);
	if (S_ISDIR(st.st_mode)) {
		printf("usb_gadget/%s already exist..\n", gname);
		return -1;
	}

	/* Connection */
	ret = mkdir(gname, O_RDWR);
	if (ret < 0) {
		printf("(trans0_1) create new gadget (mkdir:%s) fails..\n", gname);
		return ret;
	}
	ret = chdir(gname);
	if (ret < 0) {
		printf("(chdir) fails..\n");
		return ret;
	}

	char b[7];
	fd_gadget = open("idVendor", O_RDWR);             
	sprintf(b, "%s", idvendor);
	ret = write(fd_gadget, b, strlen(b));             
	close(fd_gadget);                                 

	fd_gadget = open("idProduct", O_RDWR);            
	sprintf(b, "%s", idproduct);               
	ret = write(fd_gadget, b, strlen(b));             
	close(fd_gadget);

	if (devclass != NULL) {
		fd_gadget = open("bDeviceClass", O_RDWR); 
		sprintf(b, "%s", devclass);    
		ret = write(fd_gadget, b, strlen(b));     
		close(fd_gadget);                         
	}                                                 
	
	return 0;
}

static int trans1_2(char* gname)
{
	char* config = "configs/c.1";
	char gpath[50];

	sprintf(gpath, "%s%s", g_path, gname);

	int ret = chdir(gpath);
	if (ret < 0) {
		printf("(trans1_2) chdir to %s fails..\n", gpath);
		return ret;
	}
	ret = mkdir(config, O_RDWR);
	if (ret < 0) {
		printf("(trans1_2) mkdir (configs/c.1) fails..\n");
		return ret;
	}
	return 0;
}

static int trans2_3(char* gname, char *func)
{
	char gpath[50];
	sprintf(gpath, "%s%s", g_path, gname);

	int ret = chdir(gpath);
	if (ret < 0) {
		printf("(trans2_3) chdir to %s fails..\n", gpath);
		return ret;
	}
	sprintf(gpath, "functions/%s", func);
	ret = mkdir(gpath, O_RDWR);
	if (ret < 0) {
		printf("(trans2_3) mkdir (%s) fails..\n", func);
		return ret;
	}

  if (!strcmp(gname, "hid")) 
  {
	  int fd = open("functions/hid.0/subclass", O_RDWR);
	  ret = write(fd, "1", 2);
	  close(fd);
	  fd= open("functions/hid.0/protocol", O_RDWR);
	  ret = write(fd, "1", 2);
	  close(fd);
	  fd = open("functions/hid.0/report_length", O_RDWR);
	  ret = write(fd, "8", 2);
	  close(fd);
	  fd = open("functions/hid.0/report_desc", O_RDWR);
	  ret = write(fd, report_desc_keyboard, sizeof(report_desc_keyboard));
	  close(fd);
  }
  else if (!strcmp(gname, "mass")) 
  {
	  int fd = open("functions/mass_storage.0/lun.0/file", O_RDWR);
	  ret = write(fd, "/root/lun0.img", 15);
	  close(fd);
  }
  else if (!strcmp(gname, "prt")) 
  {
    FILE *fp = fopen("functions/printer.0/q_len", "w");
    fprintf(fp, "%s", "1");
	  fclose(fp);
  }
  else if (!strcmp(gname, "tcm")) 
  {
    char *t_path = "/sys/kernel/config/target/";
	  ret = chdir(t_path);
	  if (ret < 0) {
		  printf("(trans2_3) tcm chdir to %s fails..\n", t_path);
		  return ret;
	  }

    // 0. create ramdisk and fileio
	  //ret = mkdir("core/rd_mcp_0/ramdisk", O_RDWR);
	  ret = mkdir("core/rd_mcp_0", O_RDWR);
	  ret = mkdir("core/rd_mcp_0/ramdisk", O_RDWR);
	  if (!ret) {
      FILE *fp = fopen("core/rd_mcp_0/ramdisk/control", "w");
	    if (fp == NULL) {
		    printf("fail fopen core/rd_mcp_0/ramdisk/control\n");
        return -1;
      }
      fprintf(fp, "%s", "rd_pages=32768");
	    fclose(fp);
	    fp = fopen("core/rd_mcp_0/ramdisk/enable", "w");
	    if (fp == NULL) {
		    printf("fail fopen core/rd_mcp_0/ramdisk/enable\n");
        return -1;
      }
      fprintf(fp, "%s", "1");
	    fclose(fp);
	  } 
    else 
      printf("(trans2_3) mkdir (core/rd_mcp_0/ramdisk) fails.. exist\n");

	  //ret = mkdir("core/fileio_0/fileio", O_RDWR);
	  ret = mkdir("core/fileio_0", O_RDWR);
	  ret = mkdir("core/fileio_0/fileio", O_RDWR);
	  if (!ret) {
      FILE *fp = fopen("core/fileio_0/fileio/control", "w");
	    if (fp == NULL) {
		    printf("fail fopen core/fileio_0/fileio/control\n");
        return -1;
      }
      fprintf(fp, "%s", "fd_dev_name=/root/file.bin,fd_dev_size=31457280");
	    fclose(fp);
	    fp = fopen("core/fileio_0/fileio/enable", "w");
	    if (fp == NULL) {
		    printf("fail fopen core/fileio_0/fileio/enable\n");
        return -1;
      }
      fprintf(fp, "%s", "1");
	    fclose(fp);
	  } 
    else 
      printf("(trans2_3) mkdir (core/fileio_0/fileio) fails.. exist\n");


    // 1. create two LUNs (NOTE!!! functions/tcm.0 required)
	  //ret = mkdir("usb_gadget/naa.6001405c3214b06a/tpgt_1", O_RDWR);
	  ret = mkdir("usb_gadget/", O_RDWR);
	  ret = mkdir("usb_gadget/naa.6001405c3214b06a", O_RDWR);
	  ret = mkdir("usb_gadget/naa.6001405c3214b06a/tpgt_1", O_RDWR);
	  if (ret < 0) 
		  printf("(trans2_3) mkdir (usb_gadget/naa.6001405c3214b06a/tpgt_1) fails..\n");

	  ret = mkdir("usb_gadget/naa.6001405c3214b06a/tpgt_1/lun/lun_0", O_RDWR);
	  if (ret < 0) 
		  printf("(trans2_3) mkdir (usb_gadget/naa.6001405c3214b06a/tpgt_1/lun/lun_0) fails..\n");
	  ret = mkdir("usb_gadget/naa.6001405c3214b06a/tpgt_1/lun/lun_1", O_RDWR);
	  if (ret < 0)
		  printf("(trans2_3) mkdir (usb_gadget/naa.6001405c3214b06a/tpgt_1/lun/lun_1) fails..\n");

	  FILE *fp = fopen("usb_gadget/naa.6001405c3214b06a/tpgt_1/nexus", "w");
    fprintf(fp, "%s", "naa.6001405c3214b06b");
	  fclose(fp);

    // 2. assign two LUNs 
	  int fd = open(".", O_RDONLY | O_DIRECTORY);
	  if (fd < 0) {
		  printf("(trans3_4) fail open..%d\n", fd);
		  return fd;
	  }
	  ret = symlinkat("core/rd_mcp_0/ramdisk", fd, "usb_gadget/naa.6001405c3214b06a/tpgt_1/lun/lun_0/virtual_scsi_port");
	  if (ret < 0) {
		  printf("(trans3_4) fail symlink..core/rd_mcp_0/ramdisk\n");
		  return ret;
	  }
	  ret = symlinkat("core/fileio_0/fileio", fd, "usb_gadget/naa.6001405c3214b06a/tpgt_1/lun/lun_1/virtual_scsi_port");
	  if (ret < 0) {
		  printf("(trans3_4) fail symlink..core/fileio_0/fileio\n");
		  return ret;
	  }
	  close(fd);

    // 3. enable gadget
	  fp = fopen("usb_gadget/naa.6001405c3214b06a/tpgt_1/enable", "w");
    fprintf(fp, "%s", "1");
	  fclose(fp);
  }

	return 0;
}

static int trans3_4(char* gname, char *func)
{
	char* config = "configs/c.1";
	int fd;
	char gpath[50];
	char gpath2[50];

	sprintf(gpath, "%s%s", g_path, gname);

	int ret = chdir(gpath);
	if (ret < 0) {
		printf("(trans3_4) chdir to %s fails..\n", gpath);
		return ret;
	}

	fd = open(".", O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		printf("(trans3_4) fail open..%d\n", fd);
		return fd;
	}
	sprintf(gpath, "functions/%s", func);
	sprintf(gpath2, "%s/%s", config, func);
	ret = symlinkat(gpath, fd, gpath2);
	if (ret < 0) {
		printf("(trans3_4) fail symlink..%s %s\n", func, config);
		return ret;
	}
	close(fd);

	printf("(trans3_4) success %s %s\n", func, config);
	return 0;
}

static int trans4_5(char* gname, char *dummy)
{
	char gpath[50];
	int fd;

	sprintf(gpath, "%s%s", g_path, gname);

	int ret = chdir(gpath);
	if (ret < 0) {
		printf("(trans4_5) chdir to %s fails..\n", gpath);
		return ret;
	}

	fd = open("UDC", O_RDWR);
	if (fd < 0) {
		printf("trans4_5 fail UDC open..%d\n", fd);
		return fd;
	}
	ret = write(fd, dummy, strlen(dummy) );
	if (ret < 0) {
		printf("trans4_5 fail bind write..%d\n", ret);
		return ret;
	}
	close(fd);
	return 0;
}

static int trans5_4(char* g_name)
{
	int fd, ret;
	struct stat st;

	ret = chdir(g_path);

	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode))
		return -1;

	printf("trans5_4 %s", g_name);
	ret = chdir(g_name);
	if (ret < 0) {
		printf("trans5_4 usb_gadget/%s fail\n", g_name);
    return ret;
  }

	// unbinding gadget with dummy UDC
	fd = open("UDC", O_RDWR);
	ret = write(fd, "", 1);
	close(fd);
}

static int trans4_3(char* g_name, char *func)
{
	int ret;
	struct stat st;

	ret = chdir(g_path);

	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode))
		return -1;

	printf("trans4_3 %s", g_name);
	ret = chdir(g_name);
	if (ret < 0)
		printf("trans4_3 usb_gadget/%s fail\n", g_name);

	char gpath[50];
	sprintf(gpath, "configs/c.1/%s", func);

	ret = remove(gpath);
  return ret;
}

static int trans3_2(char* g_name, char *func)
{
	struct stat st;

	int ret = chdir(g_path);

	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode))
		return -1;

	printf("trans3_2 %s\n", g_name);
	ret = chdir(g_name);
	if (ret < 0) {
		printf("trans3_2 usb_gadget/%s fail\n", g_name);
    return ret;
  }

	char gpath[50];
	sprintf(gpath, "functions/%s", func);

	ret = rmdir(gpath);
  return ret;
}

static int trans2_1(char* g_name)
{
	struct stat st;

	int ret = chdir(g_path);

	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode))
		return -1;

	printf("trans2_1 %s", g_name);
	ret = chdir(g_name);
	if (ret < 0) {
		printf("trans2_1 usb_gadget/%s fail\n", g_name);
    return ret;
  }

	ret = rmdir("configs/c.1");
	if (ret < 0) {
		printf("trans2_1 rmdir /%s fail\n", g_name);
    return ret;
  }
  return 0;
}

static int trans1_0(char* g_name)
{
	int ret = chdir(g_path);
	if (ret < 0) {
		printf("chdir to usb_gadget fails..\n");
		return ret;
	}

	struct stat st;
	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode)) {
		printf("usb_gadget/%s NOT exist..\n", g_name);
		return -1;
	}

	printf("trans1_0 %s", g_name);
	ret = rmdir(g_name);
	if (ret < 0) {
		printf("remove gadget (rmdir:%s) fails..\n", g_name);
		return ret;
	}
	return 0;
}

static int trans5_5(char* g_name, char *func)
{
	struct stat st;

	int ret = chdir(g_path);

	stat(g_name, &st);
	if (!S_ISDIR(st.st_mode))
		return -1;

	printf("trans5_5 %s", g_name);
	char gpath[50];
	sprintf(gpath, "%s/functions/%s", g_name, func);

	ret = chdir(gpath);
	if (ret < 0) {
		printf("trans5_5 usb_gadget/%s fail\n", g_name);
    return ret;
  }

  // general mutation in Ch1 : read attr
    struct dirent *de;
    char read_buf[50];
  
    // opendir() returns a pointer of DIR type.  
    DIR *dr = opendir("."); 
  
    if (dr == NULL)  // opendir returns NULL if couldn't open directory 
    { 
        printf("Could not open current directory" ); 
        return 0; 
    } 
  
    // for readdir() 
    while ((de = readdir(dr)) != NULL)  {
      printf("%s\n", de->d_name); 

      FILE *fp = fopen(de->d_name, "r");
      fscanf(fp, "%s", read_buf);
      fclose(fp);
    }
  
    closedir(dr);     

  return ret;
}

static int c1_fuzz(char *trans, char *g_dir, char *arg0, char *arg1, char *arg2)
{
  int fd, size;
  int ret;
  char buf[10];

  if (!strcmp(trans, "01")) 
    ret = trans0_1(g_dir, arg0, arg1, arg2);
  else if (!strcmp(trans, "12")) 
    ret = trans1_2(g_dir);
  else if (!strcmp(trans, "23")) 
    ret = trans2_3(g_dir, arg0);
  else if (!strcmp(trans, "34")) 
    ret = trans3_4(g_dir, arg0);
  else if (!strcmp(trans, "45")) 
    ret = trans4_5(g_dir, arg0);
  else if (!strcmp(trans, "54")) 
    ret = trans5_4(g_dir);
  else if (!strcmp(trans, "43")) 
    ret = trans4_3(g_dir, arg0);
  else if (!strcmp(trans, "32")) 
    ret = trans3_2(g_dir, arg0);
  else if (!strcmp(trans, "21")) 
    ret = trans2_1(g_dir);
  else if (!strcmp(trans, "10")) 
    ret = trans1_0(g_dir);
  else if (!strcmp(trans, "55")) 
    ret = trans5_5(g_dir, arg0);

  return ret;

}

int main (int argc, char **argv)
{
	int			c;
	int			forever = 0;
  char		*trans,	*g_dir, *arg0, *arg1, *arg2;

	while ((c = getopt (argc, argv, "t:d:h:a:b:c:")) != EOF)
	  switch (c) {
	  case 't':	
		  trans = optarg;
		  continue;
	  case 'd':
		  g_dir = optarg;
		  continue;
	  case 'a':
		  arg0 = optarg;
      if (!strlen(arg0))
      arg0 = NULL;
		  continue;
	  case 'b':
		  arg1 = optarg;
      if (!strlen(arg1))
      arg1 = NULL;
		  continue;
	  case 'c':
		  arg2 = optarg;
      if (!strlen(arg2))
      arg2 = NULL;
		  continue;
	  case 'h':
	  default:
  usage:
		  fprintf (stderr, "usage: %s\t-t transition\n", argv[0]);
		  return 1;
	  }

	if (optind != argc)
		goto usage;

  int fd2, ret;
	unsigned long *cover;
  cover = fuzzusb_kcov_init(&fd2);

  ret = c1_fuzz(trans, g_dir, arg0, arg1, arg2);

  if (cover != NULL)
    fuzzusb_kcov_end(fd2, cover);

	return ret;
}

