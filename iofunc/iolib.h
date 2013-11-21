// iolib.h
// Simple I/O library
// v1 October 2013 - shabaz

#ifndef _IOLIB_H_
#define _IOLIB_H_

#define IOLIB_DBG 0
#define DIR_IN 0
#define DIR_OUT 1

// enable pinmux functionality
// not implemented today
#define PINMUX_EN 0

#define CONTROL_MODULE 0x44e10000
#define CONTROL_LEN 0xA00
#define GPIO0 0x44e07000
#define GPIO1 0x4804c000
#define GPIO2 0x481ac000
#define GPIO3 0x481ae000
#define GPIOX_LEN 0x1000

//#define GPIO1_SIZE (GPIO1_END_ADDR - GPIO1_START_ADDR)

#define GPIO_OE 0x134
#define GPIO_SETDATAOUT 0x194
#define GPIO_CLEARDATAOUT 0x190
#define GPIO_DATAIN 0x138

extern volatile unsigned int *gpio_addr[4];
extern char* bank[2];
extern unsigned int* port_bitmask[2];

// call this first. Returns 0 on success, -1 on failure
int iolib_init(void);
// Set port direction (DIR_IN/DIR_OUT) where port is 8/9 and pin is 1-46
int iolib_setdir(char port, char pin, char dir);
// call this when you are done with I/O. Returns 0 on success, -1 on failure
int iolib_free(void);

// provides an inaccurate delay
// The maximum delay is 999msec
int iolib_delay_ms(unsigned int msec);

// set and get pin levels
inline void pin_high(char port, char pin);
inline void pin_low(char port, char pin);
inline char is_high(char port, char pin);
inline char is_low(char port, char pin);

#endif // _IOLIB_H_

