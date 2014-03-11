/*
 *  @(#) ibitstream.cc 1.8, last edit: 6/15/94 16:51:45
 *  @(#) Copyright (C) 1993, 1994 Tobias Bading (bading@cs.tu-berlin.de)
 *  @(#) Berlin University of Technology
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 *  Changes from version 1.1 to 1.2:
 *    - third argument in open syscall added
 *    - minor bug in get_header() fixed
 */
/* Hacked by F.Sartori for an OS/2 optimized version */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream.h>
#include "all.h"
#include "ibitstream.h"


#define swap_int32(int_32) (((int_32) << 24) | (((int_32) << 8) & 0x00ff0000) | \
                           (((int_32) >> 8) & 0x0000ff00) | ((int_32) >> 24))


Ibitstream::Ibitstream (int filedescriptor)             // constructor
{
    fd = filedescriptor;
    wordpointer = buffer;
    bitindex = 0;
}


Ibitstream::Ibitstream (const char *filename)           // constructor
{
    if ((fd = (int)fopen (filename, "rb")) < 0)
    {
      cerr << "can't open file \"" << filename << "\" for reading!\n";
      exit (1);
    }
    setvbuf((FILE *) fd,filebuffer,_IOFBF,FBUFSIZE);
  
    wordpointer = buffer;
    bitindex = 0;
}


Ibitstream::~Ibitstream (void)                          // destructor
{
    fclose ((FILE *)fd);
}


bool Ibitstream::get_header (uint32 *headerstring)
{
    int readvalue;
    readvalue = fread ( (char *)headerstring, 1,4,(FILE *)fd);

    if (readvalue == 0)  return False;
    if (readvalue < 0)
    {
        perror ("Reading Error");
        exit (1);
    }
#ifdef DAMN_INTEL_BYTE_ORDER
    register uint32 header = *headerstring;
    *headerstring = swap_int32 (header);
#endif
    return True;
}

bool Ibitstream::read_frame (uint32 bytesize)
{
    int readvalue;

    if (bytesize > (bufferintsize << 2))
    {
        cerr << "Internal error: framelength > bufferlength?!\n";
        exit (1);
    }
    readvalue = fread ((char *)buffer,1,bytesize,(FILE *)fd);
    if (readvalue == 0)  return False;
    if (readvalue < 0)
    {
        perror ("Reading Error");
        exit (1);
    }
    wordpointer = buffer;
    bitindex = 0;
    framesize = bytesize;
#ifdef DAMN_INTEL_BYTE_ORDER
    register uint32 *wordp, word;
    for (wordp = buffer + ((bytesize - 1) >> 2); wordp >= buffer; --wordp)
    {
        word = *wordp;
        *wordp = swap_int32 (word);
    }
#endif
  return True;
}

uint32 Ibitstream::get_bits (uint32 number_of_bits)
{
    static uint32 bitmask[17] =
    {
        0,  // dummy
        0x00000001, 0x00000003, 0x00000007, 0x0000000F,
        0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
        0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
        0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF
    };
    uint32 returnvalue;
    uint32 sum = bitindex + number_of_bits;

#ifdef DEBUG
    if (number_of_bits < 1 || number_of_bits > 16)
    {
        cerr << "illegal parameter in Ibitstream::get_bits() !\n";
        exit (1);
    }
#endif

    if (sum <= 32)
    {
        // all bits contained in *wordpointer
        returnvalue = (*wordpointer >> (32 - sum)) & bitmask[number_of_bits];
        if ((bitindex += number_of_bits) == 32)
        {
            bitindex = 0;
            if ((char *)++wordpointer > (char *)buffer + framesize)
            {
                cerr << "Ibitstream::get_bits(): no more bits in buffer!\n";
                exit (1);
            }
        }
        return returnvalue;
    }

    // bits are in *wordpointer and *(wordpointer + 1)
    // bitindex has to be > 16
#ifndef DAMN_INTEL_BYTE_ORDER
    *(int16 *)&returnvalue = *((int16 *)wordpointer + 1);
    if ((char *)++wordpointer > (char *)buffer + framesize)
    {
        cerr << "Ibitstream::get_bits(): no more bits in buffer!\n";
        exit (1);
    }
    *((int16 *)&returnvalue + 1) = *(int16 *)wordpointer;
#else
    *((int16 *)&returnvalue + 1) = *(int16 *)wordpointer;
    if ((char *)++wordpointer > (char *)buffer + framesize)
    {
        cerr << "Ibitstream::get_bits(): no more bits in buffer!\n";
        exit (1);
    }
    *(int16 *)&returnvalue = *((int16 *)wordpointer + 1);
#endif
    returnvalue >>= 48 - sum;     // returnvalue >>= 16 - (number_of_bits - (32 - bitindex))
    returnvalue &= bitmask[number_of_bits];
    bitindex = sum - 32;
    return returnvalue;
}
