/*
 *  @(#) all.h 1.6, last edit: 6/17/94 15:40:44
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

#ifndef ALL_H
#define ALL_H

typedef float		real;		// float should be enough
typedef short		bool;
typedef unsigned	uint32;		// 32 Bit unsigned integer
	// some compilers may need "typedef unsigned long uint32" instead
typedef int		int32;		// 32 Bit signed integer
	// some compilers may need "typedef long int32" instead
typedef unsigned short	uint16;		// 16 Bit unsigned integer
typedef short		int16;		// 16 Bit signed integer

typedef unsigned char	ulawsample;	// u-law byte

#ifndef True
#define True	1
#define False	0
#endif

#ifdef SunOS4_1_1
#define ULAW 1
#endif

#endif
