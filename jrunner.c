/******************************************************************/
/*                                                                */
/* Module:       jrunner.c                                        */
/*                                                                */
/* Descriptions: Main source file that manages the configuration  */
/*               processes.                                       */
/*                                                                */
/* Revisions:    1.0 02/22/02                                     */
/*               1.1 04/12/02                                     */
/*               Use jb_grabdata function instead of jb_str_cmp   */
/*               1.2 10/23/02									  */
/*				 Stratix devices JTAG ID-Code added.			  */
/*				 Change data[50] to data[CDF_PATH_LEN] because    */
/*				 length of data array must be smaller than		  */	
/*				 CDF_PATH_LEN.									  */ 		  	
/*				 1.3 1/24/2003									  */
/*				 Cyclone devices support added.					  */
/*				 1.4 5/30/2003									  */
/*				 Stratix Gx and 1C4 devices support added.		  */
/*				 ByteBlaster II support added.					  */
/*				 1.5 7/7/2004									  */
/*				 EP2S60 devices support added.					  */
/*				 SAMPLE_BSC function added.						  */
/*               MACROs added for easier source code reading      */
/*				 1.6 1/28/2005									  */
/*				 Stratix II devices support added.				  */
/*               Cyclone II EP2C35 support added.                 */
/*				 1.7 11/17/2005									  */
/*				 Cyclone II devices support added.				  */
/*				 1.8 5/29/2006									  */
/*				 Non-Altera devices bypass feature added.		  */
/*				 1.9 1/24/2007									  */
/*				 Stratix II Gx devices support added.			  */
/*				 2.00 4/26/2007									  */
/*               Cyclone III all densities/package support added. */
/*				 Moved the initialization portion out of the FOR  */
/*				 loop. This is needed to solve issue as filed in  */
/*				 SR 10588062.									  */
/*				 2.1		 06/11/07							  */
/*			     Arria GX all devices densities and package       */
/*				 support added	                                  */
/*				 Stratix III all devices densities and package    */
/*				 support added									  */
/*				 2.2	 03/12/2009								  */
/*				 Cyclone II EP2C15F256 JTAG ID-Code added		  */
/*				 Cyclone II EP2C15F484 JTAG ID-Code added		  */	
/*				 Stratix IV GX,E,GT all devices densities and	  */
/*				 support added	package							  */
/*				 2.4	24/03/2010								  */
/*				 Arria II GX and Cyclone IV all devices			  */
/*				 densities and package support added			  */
/******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jb_io.h"
#include "jb_jtag.h"
#include "jb_const.h"
#include "jb_device.h"

#if PORT==WINDOWS_NT
void SetPortMode      (int mode);
#endif /* PORT==WINDOWS_NT */

/* JRunner Controller Functions */
int  VerifyChain      ();
void DriveSignal      (int signal,int data,int clk,int buffer_enable);
void Configure        (int file_id,int dev_seq,int idcode);
void ProcessFileInput (int finputid, int idcode);
int  CheckStatus      (int dev_seq);
int	 SAMPLE_BSC		  (int dev_seq, int jtag_seq_num, int bsc_type);
void Startup          (int dev_seq);
void Help             ();

/* CDF Parser Functions */
int  ReadCDF          (int file_id);
int  GetData          (char* charset,char* buffer,char* data);
void SearchKeyword    (char* buffer,char* data);
int  CheckActionCode  (char* data);
void CheckAltDev      (int dev_seq);

/* JTAG instruction lengths of all devices */
int  ji_info[MAX_DEVICE_ALLOW];

void main(int argc, char* argv[])
{
	int i=0;
	int j=1;
	int file_id=0;
	int init_flag=0;
	int config_count=0;
	int BBMV=0;
	int BBII=0;
		
	/**********Initialization**********/

	/* Introduction */
	fprintf(stdout,"\n===================================\n");
	fprintf(stdout," JTAGRunner (JRunner) Version %s",VERSION);
	fprintf(stdout,"\n Altera Corporation ");
	fprintf(stdout,"\n JRunner %s supports", VERSION);
	fprintf(stdout,"\n ByteBlaster II and ByteBlasterMV.");
	fprintf(stdout,"\n===================================\n");

	/* initialize ji_info[MAX_DEVICE_ALLOW] */
	for(i=0;i<MAX_DEVICE_ALLOW;i++)
		ji_info[i] = 10;


	if(argc<2)
	{
		Help();
		return;
	}

	/* Open CDF file */
	file_id = jb_fopen(argv[1],"rb");

	if(file_id)
		fprintf(stdout,"Info: Chain Description File: \"%s\" opened..\n", argv[1]);
	else
	{
		fprintf(stderr,"Error: Could not open Chain Description File (CDF): \"%s\"!\n",argv[1]);
		return;
	}

	/**********Hardware Initialization**********/
	int clk = 811; // pin 11 on P8.
	if (argc>2) {
		clk = atoi(argv[2]);
	}
	if (!((clk>=800&&clk<=843)||(clk>=904&&clk<=943))) {
		fprintf(stderr,"Unacceptably strange pin given for clock %d  (%d:%d)\n",clk,clk/100,clk%100);
		fprintf(stderr,"Expect value in range 800-846 || 900-946\n");
	}
	InitGpioA(clk);

	/**********CDF Parsing**********/

	fprintf( stdout, "\nInfo: Parsing CDF...\n" );

	if(!ReadCDF(file_id))
	{
		jb_fclose(file_id);
		return;
	}

	jb_fclose(file_id);

	if(!device_count)
		return;

	for(i=0;i<device_count;i++)
	{
		if(!ji_info[i])
		{
			fprintf(stderr,"Error: JTAG instruction length of device #%d NOT specified!\n",i);
			return;
		}
		else {
			//printf("JTAG instr length = %d\n",ji_info[i]);
		}
	}

	/**********Device Chain Verification**********/

	fprintf( stdout, "Info: Verifying device chain...\n" );

	/* Verify the JTAG-compatible devices chain */
	if(VerifyChain())
		return;

	/**********Show Info***********/
	fprintf(stdout,"Debug: (1)jseq_max (2)jseq_conf_done (3)action (4)partname (5)path (6)file (7)inst_len\n");
	for(i=0;i<device_count;i++)
	{
		fprintf(stdout,"Debug: (1)%d (2)%d (3)%c (4)%s (5)%s (6)%s (7)%d\n",
			device_list[i].jseq_max,device_list[i].jseq_conf_done,device_list[i].action,device_list[i].partname,
			device_list[i].path,device_list[i].file,device_list[i].inst_len);
	}

	
	
	/**********Configuration**********/
	file_id=0;
	init_flag=0;

	for(i=1;i<device_count+1;i++)
	{
		char fullpath[CDF_PATH_LEN+CDF_FILE_LEN];

		config_count=0;

		fprintf(stderr,"In Configuration loop, lets see how it goes.\n");
		if(device_list[i-1].action != 'P')  {
			fprintf(stderr,"In Configuration loop, Skipping this cycle/device.\n");
			continue;
		}

		while(config_count<MAX_CONFIG_COUNT)
		{
			if(!config_count)
				fprintf( stdout, "\nInfo: Configuration setup device #%d\n",i);
			else
				fprintf( stdout, "\nInfo: Configuration setup device #%d (Retry# %d)\n",i, config_count);

			config_count++;
			
			/* Open programming file as READ and in BINARY */
			jb_strcpy(fullpath,device_list[i-1].path);
			jb_strcat(fullpath,device_list[i-1].file);

			file_id = jb_fopen( fullpath, "rb" );
		
			if ( file_id )
				fprintf( stdout, "Info: Programming file #%d: \"%s\" opened...\n", i,fullpath );
			else
			{
				fprintf( stderr, "Error: Could not open programming file #%d: \"%s\"\n", i,fullpath );
				return;
			}
			
			/* Start configuration */
/* [chtong,1/24/03,jrunner.c,ver 1.3] The following line has been changed */
/*			Configure(file_id,i,(device_list[i-1].idcode? JI_PROGRAM:JI_BYPASS));*/
			Configure(file_id,i,device_list[i-1].idcode);

			jb_fclose(file_id);
			
/* [chtong, 7/7/2004, jrunner.c,ver1.5] Initiailize all the devices in chain at the same time */
/* THIS PART OF CODE CAUSING THE MULTI DEVICE JTAG CHAIN WITH THE LAST DEVICE BEING BYPASSED
   NOT TO WORK AS IT SHOULD AS THE CODE WILL WAIT FOR THE LAST DEVICE TO BE PROGRAMMED 
   BEFORE PUTTING ALL DEVICES IN THE USER MODE (SR 10588062). TO FIX THIS, I MOVED THIS PORTION OUT OF THE
   FOR LOOP SO THAT IT GETS EXECUTED ONCE ALL NON-BYPASSED DEVICES IN CHAIN GET PROGRAMMED
   HAZLINA (nhramly@altera.com) APR 18th 2007. */
   
			/*
			if ( i == device_count)
			{
				int k;

				for (k=1;k<device_count+1;k++)
				{
					Startup(k);
				}
			}
			*/

			if(CheckStatus(i))
			{
				fprintf( stdout, "\nWarning: Configuration of device #%d NOT successful!\n",i );
							}
			else
			{
				config_count=MAX_CONFIG_COUNT;
				fprintf( stdout, "\nInfo: Configuration of device #%d successful...\n",i );
				init_flag++;
			}

		}
	}

/* Take the initialization portion out of the loop so that it gets executed once when every device gets programmed */
/* Hazlina (nhramly@latera.com) Apr 18th 2007 */

	if ( init_flag !=0)		/* Check if any device being programmed successfully, if not, skip this!*/
	{
		int k;

		for (k=1;k<device_count+1;k++)
		{
			Startup(k);
		}
	}

	//FreeGpioA();
	
	return;
}

/******************************************************************/
/* Name:         ReadCDF                                          */
/*                                                                */
/* Parameters:   file_id                                          */
/*               -file handler                                    */
/*                                                                */
/* Return Value: The number of devices in chain found in CDF      */
/*                                                                */
/* Descriptions: ReadCDF parses through the CDF and keeps the     */
/*               device records. It looks for 'P' or 'N' when     */
/*               mark=0. Once found, it searches for "ActionCode" */
/*               when mark=1 and once mark=2, it starts to        */
/*               retrieve the data in device record declarations. */
/*                                                                */
/******************************************************************/
int ReadCDF(int file_id)
{
	char  buffer[MAX_FILE_LINE_LENGTH];    /* line buffer */

/* [chtong,10/24/02,jrunner.c,ver1.2] Changed the following lines */
/*	char  data[50];												  
    The length of data array must be same with the longest data between 
	'(' and ')'	which is usually the path of the cdf file.
	Therefore, CDF_PATH_LEN is used								  */

	char  data[CDF_PATH_LEN];       /* device record data between '(' and ')' */
	int   mark= 0;
	
	while(jb_fgets(buffer,file_id))
	{
		//printf("DEBUG:%s",buffer);
		if (mark==1)
		{
			mark =2;

			if(GetData("ACTIONCODE",buffer,data))
			{
				if(!CheckActionCode(data))
					return 0;
			}

			device_count++;

			SearchKeyword(buffer,data);

			/* End of device record and reset flag */
			if(jb_str_cmp(";",buffer))
				mark=0;
		}
		else if (mark==2)
		{
			SearchKeyword(buffer,data);

			/* End of device record and reset flag */
			if(jb_str_cmp(";",buffer))
				mark=0;
		}
/* [sbng,4/12/02,jrunner.c,ver 1.1] The following line has been removed */
/*		else if(jb_str_cmp("P",buffer) || jb_str_cmp("N",buffer)) */
		else
		{
/* [sbng,4/12/02,jrunner.c,ver1.1] Use jb_grabdata function instead of */
/*      jb_str_cmp */
			char c_temp[50];
			
			jb_grabdata(buffer,0,1,c_temp);
						
			if(!jb_str_cmp(c_temp,"P") && !jb_str_cmp(c_temp,"N"))
				continue;
/***********************************/

			mark++;
			
			if(GetData("ACTIONCODE",buffer,data))
			{
				if(!CheckActionCode(data))
					return 0;

				/* End of device record and reset flag */
				if(jb_str_cmp(";",buffer))
				{
					device_count++;

					SearchKeyword(buffer,data);
				
					mark=0;
				}
			}
		}
/* [sbng,4/12/02,jrunner.c,ver1.1] Removed the following lines */
/*		else
		{
			continue;
		}
*/	}

	return device_count;
}

/******************************************************************/
/* Name:         GetData                                          */
/*                                                                */
/* Parameters:   charset, buffer, data                            */
/*               -charset is the character string or keyword to   */
/*                look for.                                       */
/*               -buffer is the line buffer stored when reading   */
/*                the CDF.                                        */
/*               -data is the string between brackets, '(' and ')'*/
/*                                                                */
/* Return Value: The position of the first character of charset   */
/*               found in buffer.                                 */
/*               		                                          */
/* Descriptions: The function copies the string between brackets  */
/*               right after charset into data. If charset is not */
/*               found in buffer, '0' is returned.                */
/*                                                                */
/******************************************************************/
int GetData(char* charset,char* buffer,char* data)
{
	int   char_count=0,i;
	char* buff_pointer;
	char* data_pointer;
	int   mark=0;

	char  temp[MAX_FILE_LINE_LENGTH+1];
	strcpy(temp,buffer);
	
	jb_toupper(charset);
	jb_toupper(temp);
	
	/* looking for charset in buffer */
	char_count= jb_str_cmp(charset,temp);

	/* charset not found in buffer */
	if(!char_count)
		return 0;

	data_pointer= data;
	buff_pointer= buffer;
	buff_pointer+= char_count-1+jb_strlen(charset);

	for(i=0;i<jb_strlen(buffer)-1;i++)
	{
		if(*buff_pointer=='(')
		{
			mark++;
		}
		else if(*buff_pointer==')')
		{
			if(mark==1)
			{
				fprintf(stderr,"Error: Invalid Action Code!\n");
				return 0;
			}

			/* put a null-zero to indicate the end of string to data */
			*data_pointer= '\0';
			break;
		}
		else if(mark)
		{
			mark=2;
			/* ignore '"' character */
			if(*buff_pointer!='"')
			{
				*data_pointer = *buff_pointer;
				data_pointer++;
			}
		}
		else
			return 0;

		buff_pointer++;
	}
	//fprintf(stderr,"toupper(data)=(%s) [%s]\n",data,charset);
	if (jb_strcmp("FILE",charset)!=0) {
		jb_toupper(data);
	}
	return char_count;
}

/******************************************************************/
/* Name:         CheckActionCode                                  */
/*                                                                */
/* Parameters:   data                                             */
/*               -The 3 character string indicating the action    */
/*                                                                */
/* Return Value: '0' if valid action code is detected, '1' if not */
/*                                                                */
/* Descriptions: Update the action to take in device list.        */
/*               A 'B' or a 'P' is stored for BYPASS and PORGRAM/ */
/*               CONFIGURE respectively.                          */
/*                                                                */
/******************************************************************/
int CheckActionCode(char* data)
{
	if(!jb_strcmp(data,"IGN")) {
		fprintf(stderr, "ING -> B\n");
		device_list[device_count].action= 'B';
	} else if(!jb_strcmp(data,"CFG")) {
		fprintf(stderr, "CFG -> P\n");
		device_list[device_count].action= 'P';
	} else {
		fprintf(stderr,"\nError: Invalid ActionCode: %s\n",data);
		return 0;
	}

	return 1;
}

/******************************************************************/
/* Name:         SearchKeyword                                    */
/*                                                                */
/* Parameters:   buffer, data                                     */
/*               -buffer is the line buffer stored when reading   */
/*                the CDF.                                        */
/*               -data is the string between brackets, '(' and ')'*/
/*                found in buffer.                                */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: The function search for device records corres-   */
/*               pond to part name, path, file name and           */
/*               instruction length.                              */
/*                                                                */
/******************************************************************/
void SearchKeyword(char* buffer,char* data)
{
	char  Info_name[4][20] = { "PARTNAME","PATH","FILE","INSTRUCTIONREG" };
	int   i;

	for(i=0;i<4;i++)
	{
		if(GetData(Info_name[i],buffer,data))
		{
			switch(i)
			{
			case 0:
				jb_strcpy(device_list[device_count-1].partname,data);
				CheckAltDev(device_count);
				break;
			case 1:
				jb_strcpy(device_list[device_count-1].path,data);
				break;
			case 2:
				jb_strcpy(device_list[device_count-1].file,data);
				break;
			case 3:
				device_list[device_count-1].inst_len= jb_atoi(data);
				ji_info[device_count-1]=device_list[device_count-1].inst_len;
				break;
			default:
				break;
			}
		}
	}
}

/******************************************************************/
/* Name:         CheckAltDev                                      */
/*                                                                */
/* Parameters:   dev_seq                                          */
/*               -dev_seq is the device sequence in JTAG chain.   */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: The function matches the partname specified in   */
/*               CDF with Altera devices list. If matches, the    */
/*               JTAG chain information will be updated in device */
/*               _list. The action code is updated by ActionCode. */
/*               If the partname is not recognized, the device    */
/*               will be bypassed.                                */
/*                                                                */
/******************************************************************/
void CheckAltDev(int dev_seq)
{
	int i,j,altera=0;
	dev_seq--;

	for(i=0;i<MAX_DEV_LIST;i++)
	{
		if(!jb_strcmp(device_list[dev_seq].partname,device_name[i]))
		{
			if(!(device_info[i][0] && device_info[i][1] && device_info[i][2]))
			{
				device_list[dev_seq].inst_len = device_info[i][3];
				ji_info[dev_seq]                    = device_list[dev_seq].inst_len;
				fprintf(stdout,"Warning: Device #%d not supported! Bypassed!\n",dev_seq+1);
			}
			else
			{
				device_list[dev_seq].idcode         = device_info[i][0];
				device_list[dev_seq].jseq_max       = device_info[i][1];
				device_list[dev_seq].jseq_conf_done = device_info[i][2];
				device_list[dev_seq].inst_len       = device_info[i][3];
				ji_info[dev_seq]                    = device_list[dev_seq].inst_len;
				altera = 1;

				break;
			}
		}
	}

/* [chtong,1/6/03,jrunner.c,ver 1.3] The following section has been modified*/
/* [chtong,6/2/03,jrunner.c,ver 1.4] The following section has been modified*/
/* Source code before version 1.2 cannot print family device if the device to be configured is the last family member*/
	for(j=0;j<MAX_DEV_FAMILY;j++)
	{
		if(i<start_of_device_family[j])
		{
			device_family = j-1;
			fprintf(stdout,"family: %s(%d)\n",family_name[j-1],j);
			break;
		}
		/* Source code before version 1.2 cannot print family device if the device to be configured is the last family member*/
		/* add-in && altera ==1 in version 1.4*/
		else if (j==MAX_DEV_FAMILY-1 && i>=start_of_device_family[MAX_DEV_FAMILY-1] && altera ==1)
		{
			device_family = MAX_DEV_FAMILY-1;
			fprintf(stdout, "family: %s(%d)\n",family_name[MAX_DEV_FAMILY-1],MAX_DEV_FAMILY);
		}
	}

	if(!altera)
	{
		device_list[dev_seq].idcode=0;
		device_list[dev_seq].jseq_max=0;
		device_list[dev_seq].jseq_conf_done=0;
		device_list[dev_seq].action='B';
	}
}

/******************************************************************/
/* Name:         VerifyChain                                      */
/*                                                                */
/* Parameters:   None.                                            */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: Putting all devices in BYPASS mode, a 8-bit      */
/*               vector is driven to TDI, the number of '0'       */
/*               detected indicates the number of devices in      */
/*               chain. The 8-bit vector must follows the zeroes. */
/*                                                                */
/******************************************************************/
int VerifyChain()
{
	unsigned int data=0,temp=0,test_vect=0x55;
	int i,num=0,error=0;

	Js_Reset();

	/* Load BYPASS instruction and test JTAG chain with a few vectors */
	if(Ji_Bypass(device_count,ji_info))
		return (1);
	
	Js_Shiftdr();

	/* Drive a 8-bit vector of "10101010" (right to left) to test */
	data = ReadTDO(8+device_count,test_vect,0);
	/* The number of leading '0' detected must equal to the number of devices specified */
	temp = data;
	
	for(i=0;i<device_count;i++)
	{
		temp = temp&1;
		if(temp)
			break;		
		else
			num++;
		temp = data>>(i+1);
	}

	if(temp==test_vect)	
		fprintf(stdout,"Info: Detected %d device(s) in chain...\n", num);
	else
	{
		fprintf(stderr,"Error: JTAG chain broken or #device in chain unmatch!\n");
		return (1);
	}
	
	Js_Updatedr();
	
	/* Read device IDCODE */
	Ji_Idcode(device_count,ji_info);
	Js_Shiftdr();

	for(i=device_count-1;i>=0;i--)
	{
		data = ReadTDO(CDF_IDCODE_LEN,TDI_LOW,0);

		if(device_list[i].idcode)
		{
			/* The partname specified in CDF must match with its ID Code */
			if((unsigned)device_list[i].idcode != data)
			{
				fprintf(stderr,"Error: Expected 0x%X but detected 0x%X!\n",device_list[i].idcode,data);
				error=1;
			}
			else
				fprintf(stdout,"Info: Dev%d: Altera: 0x%X\n",i+1,data);
		}
		else
		{
			fprintf(stdout,"Info: Dev%d: Non-Altera: 0x%X  (expected:%d)\n",i+1,data, device_list[i].idcode);
		}
	}
	
	Js_Updatedr();
	Js_Runidle();
	
	return error;
}

/******************************************************************/
/* Name:         Configure                                        */
/*                                                                */
/* Parameters:   file_id,dev_seq,action                           */
/*               -file_id is the ID of the file.                  */
/*               -dev_seq is the device sequence in chains.       */
/*               -action is the action to take:BYPASS or PROGRAM  */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: Issue PROGRAM instruction to the device to be    */
/*               configured and BYPASS for the rest of the devices*/
/*               Call function that processes the source file.    */
/*                                                                */
/******************************************************************/
void Configure(int file_id,int dev_seq,int idcode)
{
	int i,j,data=0;
	int action = idcode? JI_PROGRAM:JI_BYPASS;

	/* Load PROGRAM instruction */
	SetupChain(device_count,dev_seq,ji_info,action);

	if(action==JI_PROGRAM)
	{
		/* Drive TDI HIGH while moving JSM to SHIFTDR */
		DriveSignal(SIG_TDI,TDI_HIGH,TCK_QUIET,BUFFER_OFF);
		Js_Shiftdr();
		/* Issue MAX_JTAG_INIT_CLOCK clocks in SHIFTDR state */
		for(i=0;i<MAX_JTAG_INIT_CLOCK[device_family];i++)
		{
			DriveSignal(SIG_TDI,TDI_HIGH,TCK_TOGGLE,BUFFER_ON);
		}
		/* Start dumping configuration bits into TDI and clock with TCK */
		ProcessFileInput(file_id,idcode);

			/* AKL (Version1.7): Dump additional 16 bytes of 0xFF at the end of the RBF file */
			for (j=0;j<128;j++ )
			{
				DriveSignal(SIG_TDI,1,TCK_TOGGLE,BUFFER_ON);
			}

		/* Move JSM to RUNIDLE */
		Js_Updatedr();
		Js_Runidle();
	}
}

/******************************************************************/
/* Name:         CheckStatus                                      */
/*                                                                */
/* Parameters:   dev_seq                                          */
/*               -dev_seq is the device sequence in chains.       */
/*                                                                */
/* Return Value: '0' if CONF_DONE is HIGH;'1' if it is LOW.       */
/*               		                                          */
/* Descriptions: Issue CHECK_STATUS instruction to the device to  */
/*               be configured and BYPASS for the rest of the     */
/*               devices.                                         */
/*                                                                */
/*               <conf_done_bit> =                                */
/*                  ((<Maximum JTAG sequence> -                   */
/*                    <JTAG sequence for CONF_DONE pin>)*3) + 1   */
/*                                                                */
/*               The formula calculates the number of bits        */
/*               to be shifted out from the device, excluding the */
/*               1-bit register for each device in BYPASS mode.   */
/*                                                                */
/******************************************************************/
int CheckStatus(int dev_seq)
{
	int bit,data=0,error=0;
	int jseq_max=0,jseq_conf_done=0,conf_done_bit=0;

	fprintf( stdout, "Info: Checking Status\n" );

	/* Load CHECK_STATUS instruction */
	SetupChain(device_count,dev_seq,ji_info,JI_CHECK_STATUS);

	Js_Shiftdr();

	/* Maximum JTAG sequence of the device in chain */
	jseq_max= device_list[dev_seq-1].jseq_max;

	jseq_conf_done= device_list[dev_seq-1].jseq_conf_done;

	conf_done_bit = ((jseq_max-jseq_conf_done)*3)+1;

	/* Compensate for 1 bit unloaded from every Bypass register */
	conf_done_bit+= (device_count-dev_seq);
	
	for(bit=0;bit<conf_done_bit;bit++)
	{
		DriveSignal(SIG_TDI,TDI_LOW,TCK_TOGGLE,BUFFER_OFF);
	}

	data = ReadTDO(PORT_1,TDI_LOW,0);

	if(!data)
		error++;

	/* Move JSM to RUNIDLE */
	Js_Updatedr();
	Js_Runidle();

	return (error);	
}

/******************************************************************/
/* Name:         Startup                                          */
/*                                                                */
/* Parameters:   dev_seq                                          */
/*               -the device sequence in the chain.               */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: Issue STARTUP instruction to the device to       */
/*               be configured and BYPASS for the rest of the     */
/*               devices.                                         */
/*                                                                */
/******************************************************************/
void Startup(int dev_seq)
{
	int i;

	/* Load STARTUP instruction to move the device to USER mode */
	SetupChain(device_count,dev_seq,ji_info,JI_STARTUP);

	Js_Runidle();

	for(i=0;i<INIT_COUNT;i++)
	{
		DriveSignal(SIG_TCK,TCK_LOW,TCK_QUIET,BUFFER_OFF);
		DriveSignal(SIG_TCK,TCK_HIGH,TCK_QUIET,BUFFER_OFF);
	}

	/* Reset JSM after the device is in USER mode */
	Js_Reset();
}

/******************************************************************/
/* Name:         ProcessFileInput                                 */
/*                                                                */
/* Parameters:   finputid                                         */           
/*               -programming file pointer.                       */
/*                                                                */
/* Return Value: None.                                            */
/*                                                                */
/* Descriptions: Get programming file size, parse through every   */
/*               single byte and dump to parallel port.           */
/*                                                                */
/******************************************************************/
void ProcessFileInput(int finputid, int idcode)
{
	int seek_position=0,one_byte=0;
	long int file_size=0,i=0;

	/* Get file size */
	seek_position = jb_fseek(finputid,0,S_END);

	if(seek_position)
	{
		fprintf( stderr, "Error: End of file could not be located!" );
		return;
	}

	file_size = jb_ftell(finputid);
	fprintf( stdout, "Info: Programming file size: %ld\n", file_size );		
	/* Start configuration */
	/* Reset file pointer */
	jb_fseek(finputid,0,S_SET);

	fprintf(stdout,"Info: Start configuration process.\n  Please wait...\n");

	/* Loop through every single byte */
	int progress = file_size/10;
	int ct = time(0);
	for(i=0;i<file_size;i++)
	{
		if (i==progress) {
			fprintf(stdout, "Progress %7d of %7d  [%d%%] (t=%d)\n",progress,(int)file_size,(int)(progress*100.0/file_size+0.5),(int)time(0)-ct);
			progress+=file_size/10;
		}
		/*Ignore first 44 bytes in the rbf file for Cyclone device*/
		if(i<44 && ((idcode>0x2080000 && idcode<0x2086000) || (idcode>=0x20B10DD && idcode<=0x20B60DD)))		//AKL5B15: Ignore Cyclone II devices also
			one_byte = jb_fgetc(finputid);
		else
		{
			int	bit = 0,j;

			one_byte = jb_fgetc(finputid);

			/* Progaram a byte,from LSb to MSb */
			for (j=0;j<8;j++ )
			{
				bit = one_byte >> j;
				bit = bit & 0x1;
			
				/* Dump to TDI and drive a positive edge pulse at the same time */
				DriveSignal(SIG_TDI,bit,TCK_TOGGLE,BUFFER_ON);
			}
		}
	}
#if PORT==WINDOWS_NT
	/* Flush out the remaining data in Port0 */
	flush_ports();
#endif /* PORT==WINDOWS_NT */

	fprintf(stdout," done\n");
}

/******************************************************************/
/* Name:         DriveSignal                                      */
/*                                                                */
/* Parameters:   signal,data,clk,buffer_enable                    */
/*               -the name of the signal (SIG_*).                 */
/*               -the value to be dumped to the signal,'1' or '0' */
/*               -driving a LOW to HIGH transition to SIG_TCK     */
/*                together with signal.                           */
/*               -buffer_enable is used by WritePort function.	  */
/*				 -If "buffer_enable"=1,							  */
/*				 -processes in "port_io_buffer" are flushed when  */
/*               -"PORT_IO_BUFFER_SIZE" is reached.				  */	
/*				 -If "buffer_enable"=0,							  */
/*               -"data" is dumped to port 0 at once			  */
/*                                                                */
/* Return Value: None.                                            */
/*                                                                */
/* Descriptions: Dump data to signal. If clk is '1', a clock pulse*/
/*               is driven after the data is dumped to signal.    */
/*                                                                */
/******************************************************************/
void DriveSignal(int signal,int data,int clk,int buffer_enable)
{
	/* Get signal port number */
	int port = sig_port_maskbit[signal][0];

	/* Get signal mask bit*/
	int mask;
	
	/* If clk == 1, drive signal with [data] and drive SIG_TCK with '0' together. Then drive SIG_TCK with '1' */
	/* That is to create a positive edge pulse */
	if(clk)
		mask = sig_port_maskbit[signal][1] | sig_port_maskbit[SIG_TCK][1];
	else
		mask = sig_port_maskbit[signal][1];
	
	/* AND signal bit with '0', then OR with [data] */
	mask = ~mask;
	port_data[port] = (port_data[port]&mask) | (data*sig_port_maskbit[signal][1]);
	
	WritePort(port,port_data[port],buffer_enable);

	if(clk)
	{
		WritePort(port,(port_data[port] | sig_port_maskbit[SIG_TCK][1]),buffer_enable);
		WritePort(port,port_data[port],buffer_enable);
	}
}

/********************************************************************/
/*	Name : SAMPLE_BSC												*/
/*																	*/
/*	Parameter : dev_seq, jtag_seq_num, bsc_type						*/ 
/*				dev_seq:											*/		
/*					-device sequence in JTAG chain, starts from 1	*/
/*					-for the first device in JTAG chain				*/
/*				jtag_seq_num:										*/
/*					-You need to check the jtag_seq_num for the		*/
/*					-targeted IO pin from the targeted device BSDL	*/
/*					-file, which can be downloaded from				*/
/*					-www.altera.com									*/
/*					-For an example, for EP1S10F780, its INIT_DONE	*/
/*					-pin is pin W11. From the EP1S10F780 BSDL file,	*/
/*					-I/O pin W11 is BSC Group 262, so the			*/	
/*					-jtag_seq_num for pin W11 is					*/
/*					- jtag_seq_num = max_seq_num - BSC Group number	*/
/*					- jtag_seq_num = 439 - 262 = 177				*/
/*					-Thus, the jtag_seq_num for I/O pin W11 in		*/
/*					-EP1S10F780 is 177.								*/
/*				bsc_type:											*/
/*					-this parameter specifies which type of boundary*/
/*					-scan register you are interested in, whether	*/
/*					-input register, OE register or output register	*/
/*					-For each BSC, input register value is scanned	*/
/*					-out through TDO first, followed by OE register	*/
/*					-and then output register.						*/
/*																	*/
/*	Return Value: Status of the interested boundary scan register in*/
/*				  the targeted device.								*/
/*																	*/
/*  Description: This function allows you to sample and read out	*/
/*				 the value of any the boundary scan register in the	*/
/*				 scan chain.										*/
/*				 Please read AN039.pdf-IEEE1149.1 (JTAG) Boundary-  */
/*				 Scan Testing in Altera devices for more information*/
/*				 the Boundary-Scan Test (BST) circuitry in Altera	*/
/*				 devices. Don't use this function to check CONF_DONE*/
/*				 status. Instead, use CHECKSTATUS function to check */
/*				 CONF_DONE pin status.								*/
/*																	*/
/********************************************************************/
int SAMPLE_BSC(int dev_seq, int jtag_seq_num, int bsc_type)
{
	int bit,data=0;
	int jseq_max=0,jseq_target_bit=0;

	/* Load SAMPLE/PRELOAD instruction */
	SetupChain(device_count,dev_seq,ji_info,JI_SAMPLE);

	Js_Shiftdr();

	/* Maximum JTAG sequence of the device in chain */
	jseq_max= device_list[dev_seq-1].jseq_max;

	if (bsc_type==0)
		jseq_target_bit = ((jseq_max-jtag_seq_num)*3);
	else if(bsc_type ==1)
		jseq_target_bit = ((jseq_max-jtag_seq_num)*3)+1;
	else
		jseq_target_bit = ((jseq_max-jtag_seq_num)*3)+2;

	/* Compensate for 1 bit unloaded from every Bypass register */
	jseq_target_bit+= (device_count-dev_seq);
	
	for(bit=0;bit<jseq_target_bit;bit++)
	{
		DriveSignal(SIG_TDI,TDI_HIGH,TCK_TOGGLE,BUFFER_OFF);
	}

	data = ReadTDO(PORT_1,TDI_HIGH,0);

	/* Move JSM to RUNIDLE */
	Js_Updatedr();
	Js_Runidle();

	return data;	
}

/******************************************************************/
/* Name:         SetPortMode                                      */
/*                                                                */
/* Parameters:	 mode                                             */
/*				 - The mode of the port (PM_*)                    */
/*                                                                */
/* Return Value: None.                                            */
/*                                                                */
/* Descriptions: Set the parallel port registers to a particular  */
/*               values.                                          */
/*                                                                */
/******************************************************************/
void SetPortMode(int mode)
{
	/* write to Port 0 and Port 2 with predefined values */
	port_data[0] = port_mode_data[mode][0];
	port_data[2] = port_mode_data[mode][2];
	WritePort( PORT_0, port_data[0], BUFFER_OFF );
	WritePort( PORT_2, port_data[2], BUFFER_OFF );
}

/******************************************************************/
/* Name:         Help                                             */
/*                                                                */
/* Parameters:	 None.                                            */
/*                                                                */
/* Return Value: None.                                            */
/*                                                                */
/* Descriptions: Print help to standard output.                   */
/*                                                                */
/******************************************************************/
void Help()
{
	fprintf(stderr,"Error: Invalid number of argument! \nSyntax: \"jrunner <Chain Description File(.cdf)>\"\n");
	fprintf(stderr,"Example: \"jrunner project1.cdf\"\n");
}

