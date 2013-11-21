
jrunner: jb_io.c jb_jtag.c jrunner.c jb_const.h jb_device.h jb_io.h jb_jtag.h iofunc/iolib.h iofunc/iolib.c
	gcc jb_io.c jb_jtag.c jrunner.c iofunc/iolib.c -o jrunner
