jrunner-beaglebone
==================

Short:: A port to Beaglebone Black of Altera jrunner to program an Altera fpga via its jtag interface from the gpios.


This is a version of jrunner I used to program an Altera FGPA from a BeagleBone Black, using some of its GPIO to send a JTAG stream.
The program uses mmap-ed GPIO locations to achieve a decent throughput.
I understand on the Beaglebone Black, that while in principle you are able to access the gpio pins via mmap-ing, that this is not generally and universally successful.
I access the pins  via use of the iolib library, by shabaz:
http://www.element14.com/community/community/knode/single-board_computers/next-gen_beaglebone/blog/2013/10/10/bbb--beaglebone-black-io-library-for-c
However it inherit some problems from that library too. Most seriously and commonly it will crash when certain GPIO pins are used, and which depends on circumstances I have not fully understood.

Generally pins in gpio bank 1 are safe, gpio pins in bank 0 mostly work for me, while gpio pins in bank 2 and 3 mostly dont. My review of internet comments suggest 2 and 3 fail for most people, while bank 0 fails for many. I imagine there are boot sequence or device tree issues at work here.

I have found references that claim that using the device interface to export pins from the other banks moves them into an appropriate state.
I have had limited success with this, and the code will attempt to export certain pins if you call it for a bank 2 or 3 gpio.

Note, by "bank" I am refering to the variable name used in the "mode7" column of teh P8 and P9 header info pages from BBB_SRM. Eg P8:46 is listed as gpio2[7].

This code uses four pins, which it requires are sequential, although it should be simple enough for anyone needing to adjust the code to use any set of pins.
The code takes the first pin as a command line argument, which means I can list "working" pins by the start pin only.
I have found the following pins currently work as the starting/base pin on my system.
P8: pins 3-36, 41-43.
P9: pins 11-20 inclusive

Apart from the above issues, this code constitutes a relatively successful and straight forward port of the jrunner base code from Altera: https://www.altera.com/download/legacy/jrunner/dnl-jrunner.html to a Beaglebone Black system.
I have tested it for Cyclone III (EP3C16Q240) on a couple of different boards, though it should work for any FPGA that jrunner supports.

Also included are a trivial cdf file for a board with a single FPGA, of the type I was using, and a BRF file that works on that FPGA. 
You will almost certain need to edit the first and replace the second.

License:
As noted above this code includes code from jrunner, which is likely coyright Altera, tho free to distribute for use with their systems.
It also include shabaz's iolib code, which is presented without license information, but appears to be distributed for free use for any purpose.
I am adding no restrictions to the code's use of my own wrt to the original code or any changes/additions of mine, but this code quite possibly has problems beyond those listed above, and itsd possible that any such issues could cause damage or loss in the hands. Use it carefully and at you own risk, or dont use it.


Richard Plunkett


Excerpt from comment section in element14 post:
>> vegetableavenger Nov 7, 2013 12:28 AM
>> Hi shabaz,
>>  it's very useful , I really love this document .
>> Does this library will open a Repositories on github ?
>> I add some comment in this file , and i have some application conception of BBB , may need to modify/add some code and push it to github.
>> i feared that i might offend the code license of you , so I'd like to ask for your permission .
 
>> Thank you !
>> Like Show 0 Likes (0)

>  shabaz
> shabaz Nov 8, 2013 11:28 AM (in response to vegetableavenger)

> Hi Meng-Lun,
> I'm not familiar with how to create a github repository, but if you wish to do so and have any ideas to improve it, you're welcome, there is no license issue, and I will update the blog post to point to your repository as the preferred download location. Do share the link here when you get a chance.
> Thanks!
