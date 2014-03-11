
#
#  @(#) configuration.sh 1.9, last edit: 6/17/94 15:41:41
#  @(#) Copyright (C) 1993, 1994 Tobias Bading (bading@cs.tu-berlin.de)
#  @(#) Berlin University of Technology
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

#
#  Changes from 1.1 to 1.2:
#    - makedepend usage removed, so INCLUDEDIRS is empty now
#    - Linux part added by Louis P. Kruger (lpkruger@phoenix.princeton.edu)
#    - option -Wall removed
#

#
#  Remarks:
#    - Please execute "make clean" after changing settings, then "make".
#    - If you are using a SPARC that does NOT have a dbri device,
#      but a 8 kHz u-law device (amd), e.g. SPARC 1/1+/2/IPC/IPX/...,
#      add "-DULAW" to COMPILERFLAGS.
#      Don't use "-DULAW" for SPARC 10 machines, because these machines
#      can produce very fine CD-quality sound with their dbri device
#    - INCLUDEDIRS is only needed, if your C++ compiler does not know
#      where to find his own include files or if you want to use
#      makedepend. In this case, set INCLUDEDIRS to '-Iincludedir' 
#    - If you are using Linux, you may look for compiler and processor
#      specific optimization options which can be included in COMPILERFLAGS
#

###*** Please modify the entries for your machine: ***###
case `uname -sr` in
  IRIX\ 4.0.*)
     COMPILER=g++
     COMPILERFLAGS='-O2 -DIRIX -DIndigo'
     INCLUDEDIRS=
     LIBRARIES=-laudio
     AUDIO_INCLUDES='#include <audio.h>' ;;
  SunOS\ 4.1.3)
     COMPILER=g++
# for a SPARC 10:
     COMPILERFLAGS='-O2 -DSunOS -DSunOS4_1_3 -DSPARC'
# or for 8 kHz u-law output on an amd device and in stdout mode (SPARC 2/IPX/...):
#    COMPILERFLAGS='-O2 -DSunOS -DSunOS4_1_3 -DSPARC -DULAW'
     INCLUDEDIRS=
     LIBRARIES=
     AUDIO_INCLUDES='#include <sun/audioio.h>' ;;
  SunOS\ 4.1.1)
     COMPILER=g++
     COMPILERFLAGS='-O2 -DSunOS -DSunOS4_1_1 -DSPARC -DULAW'
     INCLUDEDIRS=
     LIBRARIES=
     AUDIO_INCLUDES='#include <sun/audioio.h>' ;;
  SunOS\ 5.*)
     COMPILER=g++
# or:
#    COMPILER=/usr/lang/SC2.0.1/CC
     COMPILERFLAGS='-O2 -DSolaris -DSPARC'
     INCLUDEDIRS=
     LIBRARIES=
     AUDIO_INCLUDES='#include <sys/audioio.h>' ;;
  Linux*)
     COMPILER=g++
     COMPILERFLAGS='-O2 -m486 -funroll-loops -DLINUX -DDAMN_INTEL_BYTE_ORDER'
     INCLUDEDIRS=
     LIBRARIES= 
     AUDIO_INCLUDES='#include <sys/soundcard.h>' ;;
  ULTRIX\ 4.*)
     COMPILER=g++
     COMPILERFLAGS='-O2 -DULTRIX -DDEC -DDAMN_INTEL_BYTE_ORDER'
     INCLUDEDIRS=
     LIBRARIES=
     AUDIO_INCLUDES= ;;
  *) echo "This programm has not been tested on your type of machine yet!"
     echo "Please modify the file configuration.sh according to your needs!"
     exit
esac

export COMPILER COMPILERFLAGS INCLUDEDIRS LIBRARIES

if [ ! -f audio_includes.h ]; then
  echo $AUDIO_INCLUDES >audio_includes.h
fi

make all
