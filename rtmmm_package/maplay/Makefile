
OBJS = maplay.obj ibitstream.obj header.obj scalefactors.obj subband_layer_1.obj subband_layer_2.obj \
       synthesis_filter.obj obuffer.obj crc.obj

COMPILER = gcc
CFLAGS = -Zmt -Zomf -c -O3 -funroll-loops -fstrength-reduce -fexpensive-optimizations -ffast-math -m486 -DLINUX -DDAMN_INTEL_BYTE_ORDER -D_EMX_
LINKER = gcc
LFLAGS = -Zmt -Zomf -O3 -ffast-math -m486  -Zlinker /DE -Zlinker /E:2

LIBRARIES = e:\emx\lib\gpp.lib e:\emx\lib\stdcpp.lib  -los2  -lMMPM2

.SUFFIXES:      .cc .obj

all:            maplay.exe

maplay.exe:     $(OBJS)
		$(LINKER) $(LFLAGS) $(OBJS) maplay.def -o $@ $(LIBRARIES) -lm

.cc.obj:
		$(COMPILER) -c $(CFLAGS) $(INCLUDEDIRS) $*.cc -o $@


crc.obj:                  all.h crc.h
header.obj:               all.h crc.h header.h ibitstream.h
ibitstream.obj:           all.h ibitstream.h
maplay.obj:               all.h  crc.h header.h ibitstream.h obuffer.h \
                        scalefactors.h subband.h subband_layer_1.h subband_layer_2.h \
                        synthesis_filter.h 
scalefactors.obj:         all.h scalefactors.h
obuffer.obj:              all.h  crc.h header.h ibitstream.h obuffer.h rtmmm.h
subband_layer_1.obj:      all.h  crc.h header.h ibitstream.h obuffer.h \
                        scalefactors.h subband.h subband_layer_1.h synthesis_filter.h 
subband_layer_2.obj:      all.h  crc.h header.h ibitstream.h obuffer.h \
                        scalefactors.h subband.h subband_layer_2.h synthesis_filter.h 
synthesis_filter.obj:     all.h  crc.h header.h ibitstream.h obuffer.h \
                        synthesis_filter.h 

# DO NOT DELETE THIS LINE -- make depend depends on it.
