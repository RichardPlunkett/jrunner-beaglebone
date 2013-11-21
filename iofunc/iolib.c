// iolib.c
// Simple I/O library
// v1 October 2013 - shabaz

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "iolib.h"

const unsigned int ioregion_base[]={GPIO0, GPIO1, GPIO2, GPIO3};

const char p8_bank[]={-1,-1, 1, 1, 1, 1, 2, 2, 
                       2, 2, 1, 1, 0, 0, 1, 1, 
                       0, 2, 0, 1, 1, 1, 1, 1,
                       1, 1, 2, 2, 2, 2, 0, 0,  
                       0, 2, 0, 2, 2, 2, 2,-1,
                       2, 2, 2, 2, 2, 2};
                 
const unsigned int p8_bitmask[]={
	     0,     0,  1<<6,  1<<7,  1<<2,  1<<3,  1<<2,  1<<3, 
    1<<5,  1<<4, 1<<13, 1<<12, 1<<23, 1<<26, 1<<15, 1<<14,
    1<<27,  1<<1, 1<<22, 1<<31, 1<<30,  1<<5,  1<<4,  1<<1,
    1<<0, 1<<29, 1<<22, 1<<24, 1<<23, 1<<25, 1<<10, 1<<11,
    1<<9, 1<<17,  1<<8, 1<<16, 1<<14, 1<<15, 1<<12,     0,
    1<<10, 1<<11,  1<<8,  1<<9,  1<<6,  1<<7};

const char p9_bank[]={-1,-1,-1,-1,-1,-1,-1,-1,
	                    -1,-1, 0, 1, 0, 1, 1, 1, 
	                     0, 0, 0, 0, 0, 0, 1,-1, 
	                     3,-1, 3, 3, 3,-1, 3,-1,
	                    -1,-1,-1,-1,-1,-1,-1,-1,
	                     0, 0,-1,-1,-1,-1};
	                    
const unsigned int p9_bitmask[]={
	     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0, 1<<30, 1<<28, 1<<31, 1<<18, 1<<16, 1<<19, 
    1<<5,  1<<4, 1<<13, 1<<12,  1<<3,  1<<2, 1<<17,     0,
   1<<21,     0, 1<<19, 1<<17, 1<<15,     0, 1<<14,     0,
       0,     0,     0,     0,     0,     0,     0,     0,
   1<<20,  1<<7,     0,     0,     0,     0};
	                      
int memh=0;
int ctrlh=0;
volatile unsigned int *gpio_addr[4]={NULL, NULL, NULL, NULL};
volatile unsigned int *ctrl_addr=NULL;
char* bank[2];
unsigned int* port_bitmask[2];

int
iolib_init(void)
{
	int i;
	
	if (memh)
	{
		if (IOLIB_DBG) printf("iolib_init: memory already mapped?\n");
		return(-1);
	}
	
	bank[0]=(char*)p8_bank;
	bank[1]=(char*)p9_bank;
	port_bitmask[0]=(unsigned int*)p8_bitmask;
	port_bitmask[1]=(unsigned int*)p9_bitmask;

	memh=open("/dev/mem", O_RDWR);
	for (i=0; i<4; i++)
	{
		gpio_addr[i] = mmap(0, GPIOX_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, memh, ioregion_base[i]);
		if(gpio_addr[i] == MAP_FAILED)
		{
			if (IOLIB_DBG) printf("iolib_init: gpio mmap failure!\n");
			return(-1);
		}
	}
	
	if (PINMUX_EN)
	{
		ctrl_addr = mmap(0, CONTROL_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, ctrlh, CONTROL_MODULE);
		if(ctrl_addr == MAP_FAILED)
		{
			if (IOLIB_DBG) printf("iolib_init: control module mmap failure!\n");
			return(-1);
		}
	}
	return(0);
}

int
iolib_free(void)
{
	if (memh!=0)
	{
		close(memh);
	}
	if (ctrlh!=0)
	{
		close(ctrlh);
	}
	return(0);
}

int
iolib_setdir(char port, char pin, char dir)
{
	int i;
	int param_error=0;
	volatile unsigned int* reg;
	
	// sanity checks
	if (memh==0)
		param_error=1;
	if ((port<8) || (port>>9))
		param_error=1;
	if ((pin<1) || (pin>46))
		param_error=1;
	if (bank[port][pin]<0)
		param_error=1;
	if (param_error)
	{
		if (IOLIB_DBG) printf("iolib_setdir: parameter error!\n");
		return(-1);
	}
	
	// set the bit in the OE register in the appropriate region
	if (IOLIB_DBG)
	{
		for (i=0; i<4; i++)
		{
			printf("mmap region %d address is 0x%08x\n", i, (unsigned)gpio_addr[i]);
		}
	}
	if (IOLIB_DBG) printf("iolib_setdir: bank is %d\n", bank[port-8][pin-1]);

	// lots of crashes with this lib happen here:
	// (if its is crashing here, then you are not currently allowed to write to those pins, sorry.)	
	reg=(void*)gpio_addr[bank[port-8][pin-1]]+GPIO_OE;

	if (dir==DIR_OUT)
	{
		*reg &= ~(port_bitmask[port-8][pin-1]);
	}
	else if (dir==DIR_IN)
	{
		*reg |= port_bitmask[port-8][pin-1];
	}
	
	return(0);
}

inline void
pin_high(char port, char pin)
{
	*((unsigned int *)((void *)gpio_addr[bank[port-8][pin-1]]+GPIO_SETDATAOUT)) = port_bitmask[port-8][pin-1];
}

inline void
pin_low(char port, char pin)
{
	*((unsigned int *)((void *)gpio_addr[bank[port-8][pin-1]]+GPIO_CLEARDATAOUT)) = port_bitmask[port-8][pin-1];
}

inline char
is_high(char port, char pin)
{
	return ((*((unsigned int *)((void *)gpio_addr[bank[port-8][pin-1]]+GPIO_DATAIN)) & port_bitmask[port-8][pin-1])!=0);
}

inline char
is_low(char port, char pin)
{
	return ((*((unsigned int *)((void *)gpio_addr[bank[port-8][pin-1]]+GPIO_DATAIN)) & port_bitmask[port-8][pin-1])==0);
}

int
iolib_delay_ms(unsigned int msec)
{
  int ret;
  struct timespec a;
  if (msec>999)
  {
    fprintf(stderr, "delay_ms error: delay value needs to be less than 999\n");
    msec=999;
  }
  a.tv_nsec=((long)(msec))*1E6d;
  a.tv_sec=0;
  if ((ret = nanosleep(&a, NULL)) != 0)
  {
    fprintf(stderr, "delay_ms error: %s\n", strerror(errno));
  }
  return(0);
}

