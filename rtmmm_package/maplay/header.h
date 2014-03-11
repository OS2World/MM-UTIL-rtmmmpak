/*
 *  @(#) header.h 1.7, last edit: 6/15/94 16:55:33
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

#ifndef HEADER_H
#define HEADER_H

#include "all.h"
#include "ibitstream.h"
#include "crc.h"


enum e_mode { stereo, joint_stereo, dual_channel, single_channel  };
enum e_sample_frequency { fourtyfour_point_one, fourtyeight, thirtytwo };


// Class for extraction information from a frame header:
class Header
{
  static const uint32	frequencies[3];
  uint32		h_layer, h_protection_bit, h_bitrate_index,
			 h_padding_bit, h_mode_extension;
  e_mode		h_mode;
  e_sample_frequency	h_sample_frequency;
  uint32		h_number_of_subbands, h_intensity_stereo_bound;
  bool			h_copyright, h_original;
  Crc16			*crc;
  uint16		checksum;

  uint32		calculate_framesize ();

public:
			Header (void) { crc = (Crc16 *)0; }
		       ~Header (void) { if (crc) delete crc; }
  bool			read_header (Ibitstream *, Crc16 **);
			// read a 32-bit header from the bitstream

  // functions to query header contents:
  uint32		layer (void) { return h_layer; };
  uint32		bitrate_index (void) { return h_bitrate_index; };
  e_sample_frequency	sample_frequency (void) { return h_sample_frequency; };
  uint32		frequency (void) { return frequencies[h_sample_frequency]; }
  static uint32	frequency (e_sample_frequency rate) { return frequencies[rate]; }
  e_mode		mode (void) { return h_mode; };
  bool			checksums (void) { return !h_protection_bit; }
  bool			copyright (void) { return h_copyright; }
  bool			original (void) { return h_original; }

  bool			checksum_ok (void) { return checksum == crc->checksum (); }
			// compares computed checksum with stream checksum

  // functions which return header informations as strings:
  const char *		layer_string (void);
  const char *		bitrate_string (void);
  const char *		sample_frequency_string (void);
  const char *		mode_string (void);

  uint32		number_of_subbands (void) { return h_number_of_subbands; };
			// returns the number of subbands in the current frame
  uint32		intensity_stereo_bound (void) {return h_intensity_stereo_bound; };
			// (Layer II joint stereo only)
			// returns the number of subbands which are in stereo mode,
			// subbands above that limit are in intensity stereo mode
};

#endif
