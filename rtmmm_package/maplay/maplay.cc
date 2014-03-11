#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream.h>
#include <iomanip.h>
#include <signal.h>
#include "all.h"
#include "crc.h"
#include "header.h"
#include "ibitstream.h"
#include "obuffer.h"
#include "subband.h"
#include "subband_layer_1.h"
#include "subband_layer_2.h"
#include "synthesis_filter.h"


// data extracted from commandline arguments:
static char *filename;
char *outfilename = "test.raw";
int outfd;
int outmode = S_IREAD | S_IWRITE;
static bool verbose_mode = False, filter_check = False;
static int output_mode = 1; /* use RTMMM output */
static enum e_channels which_channels = both;
static bool use_speaker = False, use_headphone = False, use_line_out = False;
static bool use_own_scalefactor = False;
static real scalefactor;

// data extracted from header of first frame:
static uint32 layer;
static e_mode mode;
static e_sample_frequency sample_frequency;

// objects needed for playing a file:
static Ibitstream *stream;
static Header *header;
static Crc16 *crc;
static Subband *subbands[32];
static SynthesisFilter *filter1 = NULL, *filter2 = NULL;
static Obuffer *buffer = NULL;


static void Exit (int returncode)
  // delete some objects and exit
{
  DosSetPriority(PRTYS_THREAD,PRTYC_REGULAR,0,0);
  delete buffer;	// delete on NULL-pointers are harmless
  delete filter1;
  delete filter2;
  delete header;
  delete stream;
  exit (returncode);
}

void breakfunc(int signo){
    Exit(99);
}


main (int argc, char *argv[])
{
  int i;
  bool read_ready = False, write_ready = False;


#if _EMX_
    struct sigaction   sa;

    sa.sa_handler = breakfunc;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    for (i=SIGHUP;i<=SIGTERM;i++)  sigaction(i,&sa,NULL);
#endif

  if (argc < 2 || !strncmp (argv[1], "-h", 2))
  {
usage:
    cerr << "\nusage: " << argv[0]
	 << " [-v] [-s] [-l] [-r] "
	    "[-c] [-f ushort] filename\n"
	    "  filename   filename of a MPEG audio stream or - for stdin\n"
	    "  -v         verbose mode\n"
	    "  -s         write pcm samples to stdout\n"
            "  -R         use RTMMM\n"
            "  -D         use DART\n"
            "  -N         use NULL device\n"
	    "  -l         decode only the left channel\n"
	    "  -r         decode only the right channel\n"
	    "  -c         check for filter range violations\n"
	    "  -f ushort  use this scalefactor instead of the default value 32768\n\n"
	    "@(#) MPEG Audio Player based on maplay 1.2 "
	    "OS/2"
	    " version)\n"
	    "@(#) Copyright (C) 1993, 1994 Tobias Bading (bading@cs.tu-berlin.de)\n"
	    "@(#) Berlin University of Technology\n"
	    "@(#) Created: 6/23/94 14:12:46\n"
	    "@(#) This program is free software. See the GNU General Public License\n"
	    "@(#) in the file COPYING for more details.\n\n"
	    "@(#) Version modified and adapted to RTMMM/2 by F.Sartori\n";
    exit (0);
  }

  // parse arguments:
  for (i = 1; i < argc; ++i)
    if (argv[i][0] == '-' && argv[i][1])
      switch ((int)argv[i][1])
      {
	case 'v':
	  verbose_mode = True;
	  break;
	case 's':
	  output_mode = 0;
	  break;
	case 'R':
	  output_mode = 1;
	  break;
	case 'D':
	  output_mode = 2;
	  break;
	case 'N':
	  output_mode = 3;
	  break;
	case 'l':
	  which_channels = left;
	  break;
	case 'r':
	  which_channels = right;
	  break;
	case 'c':
	  filter_check = True;
	  break;
	case 'f':
	  if (++i == argc)
	  {
	    cerr << "please specify a new scalefactor after the -f option!\n";
	    exit (1);
	  }
	  use_own_scalefactor = True;
	  sscanf (argv[i], "%f", &scalefactor);
	  break;
	default:
	  goto usage;
      }
    else if (!filename)
      filename = argv[i];
    else
      goto usage;

  if (!filename)
    goto usage;
  if (!(use_speaker || use_headphone || use_line_out))
    use_speaker = True;

  stream = new Ibitstream (filename);		// read from file

  header = new Header;
  if (!header->read_header (stream, &crc))
  {
    cerr << "no header found!\n";
    Exit (1);
  }

  // get info from header of first frame:
  layer = header->layer ();
  if ((mode = header->mode ()) == single_channel)
    which_channels = left;
  sample_frequency = header->sample_frequency ();
   
  // create filter(s):
  if (use_own_scalefactor)
    filter1 = new SynthesisFilter (0, scalefactor);
  else
    filter1 = new SynthesisFilter (0);
  if (mode != single_channel && which_channels == both)
    if (use_own_scalefactor)
      filter2 = new SynthesisFilter (1, scalefactor);
    else
      filter2 = new SynthesisFilter (1);

  // create buffer:
  if (output_mode==1){
    if (mode == single_channel || which_channels != both)
      buffer = new RTMMMObuffer (1,header->sample_frequency ());
    else
      buffer = new RTMMMObuffer (2,header->sample_frequency ());
    buffer->SetInputName(filename);
  }
  else
  if (output_mode==0)
    {
    outfd = open (outfilename, O_WRONLY | O_CREAT | O_BINARY, &outmode);
    if (mode == single_channel || which_channels != both)
      buffer = new FileObuffer (1);
    else
      buffer = new FileObuffer (2);
    }
  else
  if (output_mode==2)
    {
    if (mode == single_channel || which_channels != both)
      buffer = new DARTObuffer (1,header->sample_frequency ());
    else
      buffer = new DARTObuffer (2,header->sample_frequency ());
    }
  else
  if (output_mode==3)
    {
    if (mode == single_channel || which_channels != both)
      buffer = new NULLObuffer (1);
    else
      buffer = new NULLObuffer (2);
    }
  else
  {
    cerr << "Sorry, I don't know your audio device.\n"
	    "Please use the stdout mode.\n";
    Exit (0);
  }

  if (verbose_mode)
  {
    // print informations about the stream
    char *name = strrchr (filename, '/');
    if (name)
      ++name;
    else
      name = filename;
    cerr << name << " is a layer " << header->layer_string () << ' '
	 << header->mode_string () << " MPEG audio stream with";
    if (!header->checksums ())
      cerr << "out";
    cerr << " checksums.\nThe sample frequency is "
	 << header->sample_frequency_string () << " at a bitrate of "
	 << header->bitrate_string () << ".\n"
	    "This stream is ";
    if (header->original ())
      cerr << "an original";
    else
      cerr << "a copy";
    cerr << " and is ";
    if (!header->copyright ())
      cerr << "not ";
    cerr << "copyright protected.\n";
  }
  verbose_mode = 1;
  do
  { 
    // is there a change in important parameters?
    // (bitrate switching is allowed)
    if (header->layer () != layer)
    {
      // layer switching is allowed
      if (verbose_mode)
	cerr << "switching to layer " << header->layer_string () << ".\n";
      layer = header->layer ();
    }
    if ((mode == single_channel && header->mode () != single_channel) ||
	(mode != single_channel && header->mode () == single_channel))
    {
      // switching from single channel to stereo or vice versa is not allowed
      cerr << "illegal switch from single channel to stereo or vice versa!\n";
      Exit (1);
    }
    if (header->sample_frequency () != sample_frequency)
    {
      // switching the sample frequency is not allowed
      cerr << "sorry, can't switch the sample frequency in the middle of the stream!\n";
      Exit (1);
    }

    // create subband objects:
    if (header->layer () == 1)
    {
      if (header->mode () == single_channel)
	for (i = 0; i < header->number_of_subbands (); ++i)
	  subbands[i] = new SubbandLayer1 (i);
      else if (header->mode () == joint_stereo)
      {
	for (i = 0; i < header->intensity_stereo_bound (); ++i)
	  subbands[i] = new SubbandLayer1Stereo (i);
	for (; i < header->number_of_subbands (); ++i)
	  subbands[i] = new SubbandLayer1IntensityStereo (i);
      }
      else /* ! JOINT STEREO !SINGLE CHANNEL -> SIMPLE STEREO */
	for (i = 0; i < header->number_of_subbands (); ++i)
	  subbands[i] = new SubbandLayer1Stereo (i);
    }
    else if (header->layer () == 2)
    {
      if (header->mode () == single_channel)
	for (i = 0; i < header->number_of_subbands (); ++i)
	  subbands[i] = new SubbandLayer2 (i);
      else if (header->mode () == joint_stereo)
      {
	for (i = 0; i < header->intensity_stereo_bound (); ++i)
	  subbands[i] = new SubbandLayer2Stereo (i);
	for (; i < header->number_of_subbands (); ++i)
	  subbands[i] = new SubbandLayer2IntensityStereo (i);
      }
      else
	for (i = 0; i < header->number_of_subbands (); ++i)
	  subbands[i] = new SubbandLayer2Stereo (i);
    }
    else
    {
      cerr << "sorry, layer 3 not implemented!\n";
      Exit (0);
    }

//    cerr << "Reading audio data Sub-Bands #" <<header->number_of_subbands ()<<"\n";
    // start to read audio data:
    for (i = 0; i < header->number_of_subbands (); ++i)
      subbands[i]->read_allocation (stream, header, crc);

    if (header->layer () == 2)
      for (i = 0; i < header->number_of_subbands (); ++i)
	((SubbandLayer2 *)subbands[i])->read_scalefactor_selection (stream, crc);

    if (!crc || header->checksum_ok ())
    {
      // no checksums or checksum ok, continue reading from stream:
      for (i = 0; i < header->number_of_subbands (); ++i)
	subbands[i]->read_scalefactor (stream, header);

      do
      {
	for (i = 0; i < header->number_of_subbands (); ++i)
	  read_ready = subbands[i]->read_sampledata (stream);

	do
	{
	  for (i = 0; i < header->number_of_subbands (); ++i)
	    write_ready = subbands[i]->put_next_sample (which_channels, filter1, filter2);

	  filter1->calculate_pcm_samples (buffer);
	  if (which_channels == both && header->mode () != single_channel)
	    filter2->calculate_pcm_samples (buffer);
	}
	while (!write_ready);
      }
      while (!read_ready);

      buffer->write_buffer (outfd);		// write to stdout
    }
    else
      // Sh*t! Wrong crc checksum in frame!
      cerr << "WARNING: frame contains wrong crc checksum! (throwing frame away)\n";

    for (i = 0; i < header->number_of_subbands (); ++i)
      delete subbands[i];
  }
  while (header->read_header (stream, &crc));

  delete buffer;

  uint32 range_violations = filter1->violations ();
  if (mode != single_channel && which_channels == both)
   range_violations += filter2->violations ();

  if (filter_check)
  {
    // check whether (one of) the filter(s) produced values not in [-1.0, 1.0]:
    if (range_violations)
    {
      cerr << range_violations << " range violations have occured!\n";
      cerr << "If you have noticed these violations,\n";
      cerr << "please use the -f option with the value ";
      if (mode != single_channel && which_channels == both &&
	  filter2->hardest_violation () > filter1->hardest_violation ())
	cerr << filter2->recommended_scalefactor ();
      else
	cerr << filter1->recommended_scalefactor ();
      cerr << "\nor a greater value up to 32768 and try again.\n";
    }
  }
  if (verbose_mode)
  {
    // print playtime of stream:
    real playtime = filter1->seconds_played (Header::frequency (sample_frequency));
    uint32 minutes = (uint32)(playtime / 60.0);
    uint32 seconds = (uint32)playtime - minutes * 60;
    uint32 centiseconds = (uint32)((playtime - (real)(minutes * 60) - (real)seconds) * 100.0);
    cerr << "end of stream, playtime: " << minutes << ':';
    cerr.width (2);
    cerr.fill ('0');
    cerr << seconds << '.';
    cerr.width (2);
    cerr.fill ('0');
    cerr << centiseconds << '\n';
  }

  return 0;
}
