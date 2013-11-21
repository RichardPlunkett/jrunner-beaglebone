/******************************************************************/
/*                                                                */
/* Module:       jb_io.c                                          */
/*                                                                */
/* Descriptions: Manages I/O related routines, file and string    */
/*               processing functions.                            */
/*                                                                */
/* Revisions:    .0 02/22/02                                      */
/*               .1 04/11/02                                      */
/*               A new function, jb_grabdata() has been added     */ 
/*               .2 07/16/03                                      */
/*               VerifyHardware() function has been splitted into */
/*               two functions, VerifyBBII() and VerifyBBMV()     */
/*               .3 07/01/2004                                    */
/*               .4 01/08/2012                                    */
/*                                                                */
/******************************************************************/

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "jb_io.h"
#include "iofunc/iolib.h"



int BANK=8;
int BTCK=11;
int BTMS=12;
int BTDO=13;
int BTDI=14;

// Beaglebone via iolib
void pin_set(int x, int p) { if (x) pin_high(BANK,p); else pin_low(BANK,p); }
int pin_get(int p) { return is_high(BANK,p); }


void InitGpioA(int clockpin)
{
	BTCK = clockpin;
	if (BTCK>100) {
		BANK = BTCK/100;
		BTCK = BTCK%100;
	}
	BTMS=BTCK+1;	
	BTDO=BTCK+2;	
	BTDI=BTCK+3;
	
	iolib_init();
	fprintf(stderr,"Init GPIO %d :: %d %d %d %d\n",BANK,BTCK,BTMS,BTDO,BTDI);	
	fprintf(stderr,"Init GPIO pin group %d %d %d %d\n",bank[BANK-8][BTCK],bank[BANK-8][BTMS],bank[BANK-8][BTDO],bank[BANK-8][BTDI]);
	if (bank[BANK-8][BTCK]>1||bank[BANK-8][BTMS]>1||bank[BANK-8][BTDO]>1||bank[BANK-8][BTDI]>1) {
		fprintf(stderr,"Pins selected from high groups! Failure likely!!\n");
		//system("echo 5 > /sys/class/gpio/export)"; fix gpio0[x] if broken.
		system("echo 65 > /sys/class/gpio/export");
		system("echo 105 > /sys/class/gpio/export");
	}
	iolib_setdir(BANK,BTCK,DIR_OUT);
	iolib_setdir(BANK,BTMS,DIR_OUT);
	iolib_setdir(BANK,BTDO,DIR_IN);
	iolib_setdir(BANK,BTDI,DIR_OUT);

}

void FreeGpioA() 
{
	iolib_free();
}

/******************************************************************/
/* Name:         ReadPort                                         */
/*                                                                */
/* Parameters:   port                                             */
/*               -the index of port from the parallel port base   */
/*                address.                                        */
/*                                                                */
/* Return Value: Value of the port.                               */
/*               		                                          */
/* Descriptions: Read the value of the port registers.            */
/*                                                                */
/******************************************************************/
int ReadPort(int port)
{
	static int count=0;
	int data = pin_get(BTDO);
	if (data) data = 0xFF; // assert all bit, to allow recipient to pick favorite.
	return ~data; // reader seem to want things inverted ?
}

/******************************************************************/
/* Name:         WritePort                                        */
/*                                                                */
/* Parameters:   port,data,buffer_enable                          */
/*               -port is the index from the parallel port base   */
/*                address.                                        */
/*               -data is the value to dump to the port.          */
/*               -purpose of write.                               */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: Write "data" to "port" registers. When dump to   */
/*               port 0,if "buffer_enable"=1, processes in		  */
/*				 "port_io_buffer" are flushed when				  */
/*               "PORT_IO_BUFFER_SIZE" is reached			      */
/*               If "buffer_enable"=0,"data" is dumped to port 0  */
/*               at once.                                         */
/*                                                                */
/******************************************************************/
void WritePort(int port,int data,int buffer_enable)
{
	int status = 0;

	static int ock = -1;
	static int oms = -1;
	static int odi = -1;

	const int SLOW = 0;
	
	/* Put your I/O rountines here */
	int tck = !!(0x1 & data);
	int tms = !!(0x2 & data);
	int tdi = !!(0x40 & data);

	if (SLOW||ock!=tck) pin_set(tck,BTCK);
	if (SLOW||oms!=tms) pin_set(tms,BTMS);
	if (SLOW||odi!=tdi) pin_set(tdi,BTDI);

	ock=tck;
	oms=tms;
	odi=tdi;
}

/*****************************/
/*                           */
/* File processing functions */
/*                           */
/*****************************/

int jb_fopen(char* argv,char* mode)
{
	FILE* file_id;

	file_id = fopen( argv, mode );

	return (int) file_id;
}

int	jb_fclose(int file_id)
{
	fclose( (FILE*) file_id);

	return 0;
}

int jb_fseek(int finputid,int start,int end)
{
	int seek_position;

	seek_position = fseek( (FILE*) finputid, start, end );

	return seek_position;
}

int jb_ftell(int finputid)
{
	int file_size;

	file_size = ftell( (FILE*) finputid );

	return file_size;
}

int jb_fgetc(int finputid)
{
	int one_byte;

	one_byte = fgetc( (FILE*) finputid );

	return one_byte;
}

char* jb_fgets(char* buffer, int finputid)
{
	char* test;
	test=fgets(buffer,MAX_FILE_LINE_LENGTH,(FILE*) finputid);

	return test;
}

/*******************************/
/*                             */
/* String processing functions */
/*                             */
/*******************************/

/******************************************************************/
/* Name:         jb_grabdata                                      */
/*                                                                */
/* Parameters:   buffer, start_byte, term, str                    */
/*               -buffer is the line buffer stored when reading   */
/*                the CDF.                                        */
/*               -start_byte is the byte to start with on the     */
/*                buffer parsed from CDF                          */
/*               -term is how many terms to look into from the    */
/*                start_byte parsed from CDF					  */
/*				 -str is the string from start_byte till term in  */
/*				  the buffer parsed from CDF					  */
/*                                                                */
/* Return Value: mark - "0" if all spaces in the buffer, else "1" */
/*               		                                          */
/******************************************************************/
/* [sbng,4/12/02,jb_io.c ver1.1] New function added */
int jb_grabdata(char* buffer,int start_byte,int term,char* str)
{
	unsigned i=0,j=0;
	int mark=0;
	int space=0;

	if(start_byte<0 || start_byte>=(int)(strlen(buffer)-1))
	{		
		str[0]='\0';
		return (-1);
	}

	for(i=start_byte;i<strlen(buffer);i++)
	{
		if(mark==0)
		{
			if( buffer[i]!=' ' && buffer[i]!='\t' && buffer[i]!='\n' )
			{
				if(space==term-1)
					str[j++]=buffer[i];
				mark=1;
			}
			else if( buffer[i]==' ' || buffer[i]=='\t' )
				mark=0;
			else
			{
				if(!((buffer[i]==' ' || buffer[i]=='\t') && (buffer[(i>0)? i-1:0]==' ' || buffer[i]=='\t' )))
					space++;
				if(space>term-1)
					break;
			}
		}
		else if(mark==1)
		{
			if( buffer[i]!=' ' && buffer[i]!='\t' && buffer[i]!='\n' )
			{
				if(space==term-1)
					str[j++]=buffer[i];
			}
			else
			{
				if(!((buffer[i]==' ' || buffer[i]=='\t' ) && (buffer[(i>0)? i-1:0]==' ' || buffer[i]=='\t' )))
					space++;
				if(space>term-1)
					break;
			}
		}
		else;
	}

	str[j]='\0';

	return mark;
}

void jb_strcpy(char* a,char* b)
{
	strcpy(a,b);
}


int jb_str_cmp(char* charset,char* buffer)
{
	char* pc;
	char  temp[MAX_FILE_LINE_LENGTH+1],store;
	unsigned int i;

	char *s=strstr(buffer,charset);	
	if (!s) return 0;
	else return s-buffer+1;

}

int jb_strlen(const char* str)
{
	return strlen(str);
}

/******************************************************************/
/* Name:         jb_strcmp                                        */
/*                                                                */
/* Parameters:   a, b			                                  */
/*               -a and b are strings							  */
/*                                                                */
/* Return Value: -0 if a and b are identical                      */
/*				 -!0 if a and b are not identical				  */
/*               		                                          */
/* Descriptions: Used to compare strings						  */
/*                                                                */
/******************************************************************/
int jb_stricmp(char* a,char* b)
{
	while (*a || *b) {
		if (toupper(*a)<toupper(*b)) return -1;
		if (toupper(*a)>toupper(*b)) return 1;
	}
	return 0;
}

int jb_strcmp(char* a,char* b)
{
	//fprintf(stderr,"strcmp [%s][%s]\n",a,b);
	return strcmp(a,b);
}

void jb_strcat(char* dst,char* src)
{
	strcat(dst,src);
}

int jb_atoi(char* number)
{
	return atoi(number);
}

void jb_toupper(char* str)
{
	char* pstr;

	pstr = str;

	while(*pstr)
	{
		if(*pstr>='a' && *pstr<='z')
			*pstr -= 0x20;

		pstr++;
	}
}
