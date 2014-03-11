#ifndef OBUFFER_H
#define OBUFFER_H

#define  INCL_OS2MM                 /* required for MCI and MMIO headers   */
#define  INCL_DOS

#include <os2.h>
#include <os2me.h>
#include <unistd.h>
#include <stdlib.h>
#include "rtmmm.h"
#include "all.h"
#include "header.h"

static const uint32 MAXCHANNELS = 2;            // max. number of channels
static const uint32 NUMBER_OF_LOCAL_BUFFERS = 16; /* max 1Mbytes if DART = 64K */
static const uint32 NUMBER_OF_LOCAL_BUFFERS_MASK = 0xF;

// Abstract base class for audio output classes:
class Obuffer
{
public:
    virtual     ~Obuffer (void) {}                // dummy
    virtual void append (uint32 channel, int16 value) = 0;
                 // this function takes a 16 Bit PCM sample
    virtual void write_buffer (int fd) = 0;
                 // this function should write the samples to the filedescriptor
                 // or directly to the audio hardware
    virtual void SetInputName(char *s) = 0;
};

typedef struct{
    int   bufferindex;
    int16 *buffer;
    int   index;
} index_type;

class RTMMMObuffer : public Obuffer
{
private:
    // client interface
    int                     subbuffer_size;    /* the size of a subbuffer */
    int                     subbuffer_mask;    /* to allow circular addressing within subbuffer */
    index_type              buffer_index[MAXCHANNELS]; /* to allow r-l write on buffers */
    int16                   *subbuffers[NUMBER_OF_LOCAL_BUFFERS + 2]; /* points to the DART buffers */
    int                     buffer_mask;       /* to allow circular addressing among subbuffers */
    int                     channels;          /* 1 = mono 2 = stereo */
    // buffers interface
    int                     next_play_buffer;  /* the next buffer that must be sent to DART */
    int                     buffers_counter_mask; /* the mask to rotate among buffers */
    int                     ready_buffers;     /* the number of ready buffers */
    int                     free_buffers;      /* the number of freed buffers */
    HEV                     waitsem;
    int                     music_interface;   // 0 = RTMMM 1 = DART 2 = NULL
    // RTMMM interface
    HQUEUE                  server_queue;
    RTMMM_InputChannel_type *OutputChannel;
    RTMMM_Definition_type   *MixerInfo;
    int                     thread_id;
    int                     server_id;
    // DART Backup parameters
    MCI_MIXSETUP_PARMS      MixSetupParms;     /* Mixer parameters     */
    MCI_AMP_OPEN_PARMS      AmpOpenParms;      /* Parameters of the opening ofthe device */
    MCI_BUFFER_PARMS        BufferParms;       /* Device buffer parms  */

public:
          RTMMMObuffer (uint32 number_of_channels,uint32 sampling_frequency);
         ~RTMMMObuffer (void);
    void  append (uint32 channel, int16 value);
    void  write_buffer (int fd); /* NOT USED HERE: IT IS EMPTY */
    void  RTMMMSynchTask(void);
    void  SetInputName(char *s);
};

static const uint32 LOCAL_BUFFERS_SIZE = 0x4000;  // 16384 samples (16 bit each)
static const uint32 LOCAL_BUFFERS_MASK = 0x3fff;
static const uint32 LOCAL_BUFFERS_BITS = 15;
static const uint32 frequencies[3] = { 44100, 48000, 32000 };

class DARTObuffer : public Obuffer
{
private:
    MCI_MIXSETUP_PARMS MixSetupParms;     /* Mixer parameters     */
    MCI_AMP_OPEN_PARMS AmpOpenParms;      /* Parameters of the opening of the device */
    MCI_BUFFER_PARMS   BufferParms;       /* Device buffer parms  */
    int                subbuffer_size;    /* the size of a subbuffer */
    index_type         buffer_index[MAXCHANNELS]; /* to allow r-l write on buffers */
    int16              *subbuffers[NUMBER_OF_LOCAL_BUFFERS + 2]; /* points to the DART buffers */
    int                subbuffer_mask;    /* to allow circular addressing within subbuffer */
    int                buffer_mask;       /* to allow circular addressing among subbuffers */
    int                channels;          /* 1 = mono 2 = stereo */
    HEV                waitsem;           /* to reactivate the filling of the buffer when there is free space */


public:
          DARTObuffer (uint32 number_of_channels,uint32 sampling_frequency);
         ~DARTObuffer (void);
    void  append (uint32 channel, int16 value);
    void  write_buffer (int fd); /* NOT USED HERE: IT IS EMPTY */
    void  SetInputName(char *s){};
};

static const uint32 OBUFFERSIZE = 2 * 1152;     // max. 2 * 1152 samples per frame

class FileObuffer : public Obuffer
{
private:
    int16 buffer[OBUFFERSIZE];
    int16 *bufferp[MAXCHANNELS];
    uint32 channels;

public:
        FileObuffer (uint32 number_of_channels);
       ~FileObuffer (void) {}
    void  append (uint32 channel, int16 value);
    void  write_buffer (int fd);
    void  SetInputName(char *s){};
};


class NULLObuffer : public Obuffer
{
private:

public:
        NULLObuffer (uint32 number_of_channels);
       ~NULLObuffer (void) {}
    void  append (uint32 channel, int16 value);
    void  write_buffer (int fd);
    void  SetInputName(char *s){};
};

#endif