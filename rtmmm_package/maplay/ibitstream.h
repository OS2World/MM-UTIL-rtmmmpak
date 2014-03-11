/*
 *  @(#) ibitstream.h 1.5, last edit: 6/15/94 16:55:34
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

#ifndef BITSTREAM_H
#define BITSTREAM_H

#include "all.h"

const uint32 bufferintsize = 433;
	// max. 1730 bytes per frame: 144 * 384kbit/s / 32000 Hz + 2 Bytes CRC
const int FBUFSIZE = 16*1024; /* 128k buffer */

// Class to extract bitstrings from files:
class Ibitstream
{
  int		fd;
  char          filebuffer[FBUFSIZE];
  uint32	buffer[bufferintsize];
  uint32	framesize;		// number of valid bytes in buffer
  uint32	*wordpointer;		// position of next unsigned int for get_bits()
  uint32	bitindex;		// number (0-31, from MSB to LSB) of next bit for get_bits()
  

public:
		Ibitstream (int filedescriptor);
		Ibitstream (const char *filename);
		~Ibitstream (void);
  int		filedescriptor (void) { return fd; };

  bool		get_header (uint32 *);
		// get next 32 bits from bitstream in an unsigned int,
		// returned value False => end of stream
  bool		read_frame (uint32 bytesize);
		// fill buffer with data from bitstream, returned value False => end of stream
  uint32	get_bits (uint32 number_of_bits);
		// read bits (1 <= number_of_bits <= 16) from buffer into the lower bits
		// of an unsigned int. The LSB contains the latest read bit of the stream.
};

#endif
