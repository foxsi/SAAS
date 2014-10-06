# Pattern matching in make files
#   $^ matches all the input dependencies
#   $< matches the first input dependency
#   $@ matches the output

CC=g++
GCC_VERSION_GE_43 := $(shell expr `g++ -dumpversion | cut -f2 -d.` \>= 3)

PUREGEV_ROOT = /opt/pleora/ebus_sdk
OPENCVDIR = /usr/include/opencv2/
#CCFITSDIR = /usr/include/CCfits/
INCLUDE = -I$(OPENCVDIR) -I$(PUREGEV_ROOT)/include/ -I$(CCFITSDIR)

CFLAGS = -Wall $(INCLUDE) -Wno-unknown-pragmas
ifeq "$(GCC_VERSION_GE_43)" "1"
    CFLAGS += -std=gnu++0x
endif

IMPERX =-L$(PUREGEV_ROOT)/lib/		\
	-lPvBase             		\
	-lPvDevice          		\
	-lPvBuffer          		\
	-lPvPersistence      		\
	-lPvGenICam          		\
	-lPvStreamRaw        		\
	-lPvStream 
THREAD = -lpthread
X11 = -lX11
GL = -lGL
GLU = -lGLU
GLUT = -lglut

EXEC_CORE = snap
EXEC_ALL = $(EXEC_CORE) 

default: $(EXEC_CORE)

all: $(EXEC_ALL)

snap: snap.cpp ImperxStream.o compression.o
	$(CC) $(CFLAGS) $^ -o $@ $(IMPERX)

stream: stream.cpp
	$(CC) $(CFLAGS) $^ -o $@ $(IMPERX)

display: display.cpp
	$(CC) $(CFLAGS) $^ -o $@ $(GL) $(GLU) $(GLUT)

#This pattern matching will catch all "simple" object dependencies
%.o: %.cpp %.hpp
	$(CC) -c $(CFLAGS) $< -o $@

install: $(EXEC_CORE)
	#sudo systemctl stop sas
	#sudo systemctl stop sbc_info
	#sudo systemctl stop sbc_shutdown
	#sudo cp sunDemo /usr/local/bin/sas.runtime
	#sudo cp sbc_info /usr/local/bin/sbc_info.runtime
	#sudo cp sbc_shutdown /usr/local/bin/sbc_shutdown.runtime
	#sudo systemctl start sbc_shutdown
	#sudo systemctl start sbc_info
	#sudo systemctl start sas

clean:
	rm -rf *.o *.out $(EXEC_ALL)
