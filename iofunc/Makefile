#
# Execute 'make' to create libiofunc.a and the test programs
# Other options:
# make clean
# make all
# make lib
#

lib = libiofunc.a
test = test_app

LIB_PATH = .    
LIBRARIES = iofunc
INCLUDES = -I. ${LIB_PATH}

SOURCES =  iolib.c
OBJECTS = $(SOURCES:%.c=%.o) 

EXTRA_DEFINE = 
CCCFLAGS = $(EXTRA_DEFINE) 
CC = gcc
CFLAGS = $(EXTRA_DEFINE)

all : libiofunc.a $(test)
lib : $(lib)
test: $(test)

$(test) : $(test:%=%.c)
	$(CC) $(CFLAGS) -c -o $@.o $@.c
	$(CC) $@.o $(LIB_PATH:%=-L%) $(LIBRARIES:%=-l%) -o $@

clean :
	rm -rf *.o libiofunc.a $(test) core *~

libiofunc.a : $(OBJECTS)
	ar -rs libiofunc.a $(OBJECTS)

.SUFFIXES: .c.d

%.d: %.c
	$(SHELL) -ec "$(CC) -M $(CPPFLAGS) $< | sed 's/$*\\.o[ :]*/$@ &/g' > $@" -include $(SOURCES:.c=.d)

