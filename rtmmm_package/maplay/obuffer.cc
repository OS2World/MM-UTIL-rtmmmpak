#define  INCL_OS2MM                 /* required for MCI and MMIO headers   */
#define  INCL_DOS

#include <os2.h>
#include <os2me.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream.h>
#include "obuffer.h"
#include "header.h"

#include <stdio.h>

int missed = 0;
int played = 0;


static struct{
    ULONG               mix_handle;
    MCI_MIX_BUFFER      *MixBuffers;
    MCI_MIX_BUFFER      *EmptyBuffer;
    MCI_MIXSETUP_PARMS  *MixSetupParms;
    int                 next_play_buffer; /* the next buffer that must be sent to DART */
    int                 buffers_counter_mask; /* the mask to rotate among buffers */
    int                 ready_buffers;    /* the number of ready buffers */
    int                 free_buffers;     /* the number of freed buffers */
    PHEV                waitsem;          /* The semaphore to Sync the mixer with the player */
} DART_Local;

LONG APIENTRY DART_SendBuffers (ULONG ulStatus,PMCI_MIX_BUFFER pBuffer,ULONG ulFlags){
    if (( ulFlags & MIX_WRITE_COMPLETE) !=0)
    {
       if (DART_Local.ready_buffers > 0)
       {
           DART_Local.MixSetupParms->pmixWrite(DART_Local.mix_handle,
               &DART_Local.MixBuffers[DART_Local.next_play_buffer],DART_Local.ready_buffers);
           DosEnterCritSec();
           DART_Local.next_play_buffer += DART_Local.ready_buffers;
           DART_Local.next_play_buffer &= DART_Local.buffers_counter_mask;
           DART_Local.ready_buffers = 0;
           DART_Local.free_buffers  ++;
           DosExitCritSec();
           DosPostEventSem(*DART_Local.waitsem);
       } else
       {
           DART_Local.MixSetupParms->pmixWrite(DART_Local.mix_handle,
               &DART_Local.EmptyBuffer[0],1);
       }
   }
   return( TRUE );
} /* end MyEvent */

void RTMMMObufferThread(void *args){
    RTMMMObuffer *p;

    DosSetPriority(PRTYS_THREAD,PRTYC_TIMECRITICAL,-2,0);

    p = (RTMMMObuffer *)args;
    p->RTMMMSynchTask();
}

/******************************************************************************/
/*                                                                            */
/*    RTMMMObuffer                                                            */
/*                                                                            */
/******************************************************************************/

RTMMMObuffer::RTMMMObuffer (uint32 number_of_channels,uint32 sampling_frequency)
{
    int           SubbufferByteSize;  // byte size of a subbuffer
    int           SamplesPerBuffer;   // number of samples in a buffer (one channel)
    int           SharMemSize;
    PSZ           SharedMemName = (PSZ)RTMMM_MEMORY;
    PSZ           MessageQueueName = (PSZ)RTMMM_QUEUE;
    unsigned int  i;
    APIRET        ret;
    char          errs[100];

    cerr << " Opening RTMMM with " << frequencies[sampling_frequency] << " hertz and "<< number_of_channels << " Channels \n";

    ret = DosGetNamedSharedMem((void **)&MixerInfo,SharedMemName,PAG_READ);
    if (ret!=0) cerr << " Cannot find RTMM server \n";

    if (ret==0) {  // found RTMMM interface: use it!
        float   k_filt,gain;
        int     frequency_ratio;

        music_interface = 0; // RTMMM

        frequency_ratio = (256 * frequencies[sampling_frequency]) / MixerInfo->MixSetupParms.ulSamplesPerSec;

        SubbufferByteSize = (MixerInfo->buffers_size * number_of_channels) / MixerInfo->MixSetupParms.ulChannels;
        SamplesPerBuffer = SubbufferByteSize / number_of_channels;
        if (MixerInfo->MixSetupParms.ulBitsPerSample == 16) SamplesPerBuffer /= 2;
        SamplesPerBuffer *= frequency_ratio;
        SamplesPerBuffer >>= 8;
        SubbufferByteSize = SamplesPerBuffer * number_of_channels * 2; /* 16 bit */

        k_filt = (float)frequencies[sampling_frequency] / (float)MixerInfo->MixSetupParms.ulSamplesPerSec;
//        k_filt *= k_filt;
        gain   = 0.5;

        cerr <<  frequency_ratio << " Frequency Ratio  * 256 \n";
        cerr <<  SubbufferByteSize << " SubbufferByteSize \n";
        cerr <<  SamplesPerBuffer << " Samples per buffer = "<<(1000*SamplesPerBuffer)/(frequencies[sampling_frequency] * number_of_channels)<<" msecs \n";

        SharMemSize  = sizeof(RTMMM_InputChannel_type) +
            (NUMBER_OF_LOCAL_BUFFERS + 2) * SubbufferByteSize;

        ret = DosAllocSharedMem((void **)&OutputChannel,NULL,
                SharMemSize,PAG_COMMIT | PAG_WRITE | OBJ_GETTABLE | OBJ_GIVEABLE);
        if (ret!=0){
            cerr << "Fatal Error " << ret << " creating client shared memory \n";
            exit(-1);
        }

        for (i=0;i<NUMBER_OF_LOCAL_BUFFERS + 2;i++){
            subbuffers[i] = (int16 *) ((int)OutputChannel +
                sizeof(RTMMM_InputChannel_type) + SubbufferByteSize * i);
            memset(subbuffers[i],'\0', SubbufferByteSize);
        }
        OutputChannel->frequency_ratio  = frequency_ratio * number_of_channels;
        OutputChannel->size             = 16;
        OutputChannel->channels         = number_of_channels;
        OutputChannel->buffer           = NULL;
        OutputChannel->mute             = 1; /* NOT MUTED */
        OutputChannel->k[0]             = (int)(32768.0 * k_filt * gain);
        OutputChannel->k[1]             = (int)(32768.0 * k_filt * gain);
        OutputChannel->k[2]             = (int)(32768.0 * (1-k_filt));
        OutputChannel->old_output[0]    = 0;
        OutputChannel->old_output[1]    = 0;
        strncpy(OutputChannel->music_name,"MAPLAY_RTMMM",79);

        ret = DosCreateEventSem(NULL,&OutputChannel->syncsem,DC_SEM_SHARED,0);
        if (ret!=0){
            cerr << "Fatal Error " << ret << " creating server sem \n";
            exit(-1);
        }

        ret = DosOpenQueue((long unsigned int *)&server_id,&server_queue,MessageQueueName);
        if (ret!=0){
            cerr << "Fatal Error " << ret << " opening queue \n";
            exit(-1);
        }

        ret = DosWriteQueue(server_queue,RTMMM_CONNECT,1,OutputChannel,0);
        if (ret!=0){
            cerr << "Fatal Error " << ret << " writing queue \n";
            exit(-1);
        }
        subbuffer_size                  = SamplesPerBuffer * number_of_channels;
        subbuffer_mask                  = 0x0fffffff; /* max 24 Mbytes !!!! */
        while (subbuffer_mask > subbuffer_size) subbuffer_mask >>=1;

        next_play_buffer                = NUMBER_OF_LOCAL_BUFFERS - 2;
        buffers_counter_mask            = NUMBER_OF_LOCAL_BUFFERS_MASK;
        ready_buffers                   = 2;
        free_buffers                    = NUMBER_OF_LOCAL_BUFFERS - 4;

    } else { // try DART
        music_interface = 1; // DART

        cerr << " Opening DART interface \n";

        memset(&MixSetupParms,'\0',sizeof(MCI_MIXSETUP_PARMS));
        memset(&AmpOpenParms,'\0',sizeof(MCI_AMP_OPEN_PARMS));

        AmpOpenParms.usDeviceID = ( USHORT ) 0;
        AmpOpenParms.pszDeviceType = ( PSZ ) MCI_DEVTYPE_AUDIO_AMPMIX;
        ret = mciSendCommand( 0,MCI_OPEN,MCI_WAIT|MCI_OPEN_TYPE_ID,(void *) &AmpOpenParms,0);
        mciGetErrorString(ret,(PSZ)errs,1023);
        if (ret!=0){
            cerr << "Fatal Error " << errs  << " Opening DART \n";
            exit(-1);
        }

        MixSetupParms.hwndCallback      = 0;
        MixSetupParms.ulBitsPerSample   = 16;
        MixSetupParms.ulFormatTag       = MCI_WAVE_FORMAT_PCM;
        MixSetupParms.ulSamplesPerSec   = frequencies[sampling_frequency];
        MixSetupParms.ulChannels        = number_of_channels;
        MixSetupParms.ulFormatMode      = MCI_PLAY;
        MixSetupParms.ulDeviceType      = MCI_DEVTYPE_WAVEFORM_AUDIO;
        MixSetupParms.ulMixHandle       = 0; /* RETURN value here */
        MixSetupParms.pmixEvent         = DART_SendBuffers;  /* CallBack routine */
        MixSetupParms.pExtendedInfo     = NULL; /* NO Ext Info */

        ret = mciSendCommand(AmpOpenParms.usDeviceID,MCI_MIXSETUP,MCI_WAIT|MCI_MIXSETUP_INIT,
                             (void *) &MixSetupParms,0);
        mciGetErrorString(ret,(PSZ)errs,1023);
        if (ret!=0){
            cerr << "Fatal Error " << errs  << " Setting up DART \n";
            exit(-1);
        }

        /* buffer handling */
        BufferParms.ulNumBuffers        = NUMBER_OF_LOCAL_BUFFERS + 2;
        BufferParms.ulBufferSize        = LOCAL_BUFFERS_SIZE * 2;
        BufferParms.pBufList            = (MCI_MIX_BUFFER *)malloc(sizeof(MCI_MIX_BUFFER)*(NUMBER_OF_LOCAL_BUFFERS + 2));

        ret = mciSendCommand(AmpOpenParms.usDeviceID,MCI_BUFFER,MCI_WAIT|MCI_ALLOCATE_MEMORY,
                             (void *) &BufferParms,0);
        mciGetErrorString(ret,(PSZ)errs,1023);
        if (ret!=0){
            cerr << "Fatal Error " << errs  << " Allocating DART Memory \n";
            exit(-1);
        }

        for (i=0; i < (NUMBER_OF_LOCAL_BUFFERS + 2) ; i++)
        {
            subbuffers[i] = (int16 *)((MCI_MIX_BUFFER *)BufferParms.pBufList)[i].pBuffer;
            memset(subbuffers[i],'\0',LOCAL_BUFFERS_SIZE * 2);
        }

        subbuffer_mask                 = LOCAL_BUFFERS_MASK;
        subbuffer_size                 = LOCAL_BUFFERS_SIZE;
    }

    channels                        = number_of_channels;
    buffer_mask                     = NUMBER_OF_LOCAL_BUFFERS_MASK;

    for (i=0; i<MAXCHANNELS; i++)
    {
        buffer_index[i].bufferindex = 0;
        buffer_index[i].buffer      = (int16 *)subbuffers[0];
        buffer_index[i].index       = i;
    }

    ret = DosCreateEventSem(NULL,&waitsem,0L,0);
    if (ret!=0) cerr << "Error " << ret << " creating event sem \n";

    if (music_interface == 0) {
        DosCreateThread((long unsigned int *)&thread_id,(PFNTHREAD)RTMMMObufferThread,
               (ULONG)this, CREATE_READY | STACK_COMMITTED,32768);
        if (ret!=0){
            cerr << "Fatal Error " << ret << " creating thread \n";
            exit(-1);
        }
    } else {
        DART_Local.mix_handle          = MixSetupParms.ulMixHandle;
        DART_Local.MixBuffers          = (MCI_MIX_BUFFER *)BufferParms.pBufList;
        DART_Local.EmptyBuffer         = &((MCI_MIX_BUFFER *)BufferParms.pBufList)[NUMBER_OF_LOCAL_BUFFERS];
        DART_Local.next_play_buffer    = NUMBER_OF_LOCAL_BUFFERS - 2;
        DART_Local.buffers_counter_mask= NUMBER_OF_LOCAL_BUFFERS_MASK;
        DART_Local.ready_buffers       = 2;
        DART_Local.free_buffers        = NUMBER_OF_LOCAL_BUFFERS - 4;
        DART_Local.waitsem             = &waitsem;
        DART_Local.MixSetupParms       = &MixSetupParms;
        /* Kickoff dart */
        MixSetupParms.pmixWrite(MixSetupParms.ulMixHandle,&DART_Local.MixBuffers[NUMBER_OF_LOCAL_BUFFERS-4], 2);
    }

    /* Set priority to high */
//    DosSetPriority(PRTYS_THREAD,PRTYC_FOREGROUNDSERVER,0,0);
    DosSetPriority(PRTYS_THREAD,PRTYC_TIMECRITICAL,-3,0);
}

RTMMMObuffer::~RTMMMObuffer (void){
    MCI_GENERIC_PARMS GenericParms;
    int               ret;

    if (music_interface == 0) // RTMMM
    {
        while(ready_buffers > 0)
        {
            /* WAIT FOR NEXT PLAYED BUFFER*/
            DosWaitEventSem(waitsem,SEM_INDEFINITE_WAIT); /* wait */
        }

        DosWriteQueue(server_queue,RTMMM_DISCONNECT,10,OutputChannel,0);

        DosKillThread((TID)thread_id);

        DosCloseEventSem(waitsem);

        DosCloseEventSem(OutputChannel->syncsem);

        DosFreeMem(MixerInfo);

        DosFreeMem(OutputChannel);
    } else  // DART
    {
        while(DART_Local.ready_buffers !=0)
        {
            /* WAIT FOR NEXT PLAYED BUFFER*/
            DosWaitEventSem(waitsem,SEM_INDEFINITE_WAIT); /* wait */
        }

        DosCloseEventSem(waitsem);

        ret = mciSendCommand(AmpOpenParms.usDeviceID,MCI_BUFFER,MCI_WAIT|MCI_DEALLOCATE_MEMORY,
                            (void *) &BufferParms,0);
        if (ret!=0) cerr << "Error " << ret << " De-allocating DART Memory \n";

        ret = mciSendCommand(AmpOpenParms.usDeviceID,MCI_CLOSE,MCI_WAIT ,
                            (void *)&GenericParms,0);
        if (ret!=0) cerr << "Error " << ret << " Closing DART  \n";
    }
}

void RTMMMObuffer::append (uint32 channel, int16 value)
{
    register int index = buffer_index[channel].index;

    buffer_index[channel].buffer[index] = value;

    index+=channels;

    if (index >= subbuffer_size)
    {
        register int bufferindex = buffer_index[channel].bufferindex;

        index -= subbuffer_size; // was index &= subbuffer_mask
        bufferindex++;
        bufferindex &= buffer_mask;

        buffer_index[channel].bufferindex = bufferindex;
        buffer_index[channel].buffer      = subbuffers[bufferindex];

        DosEnterCritSec();
        /* can I release a buffer as done? */
        if (music_interface == 0) // RTMMM
        {
            if (channels == 1)
            {
                ready_buffers++;
                free_buffers--;
            } else
            {
                if (buffer_index[0].bufferindex == buffer_index[1].bufferindex)
                    ready_buffers++;
                if (buffer_index[0].bufferindex != buffer_index[1].bufferindex)
                    free_buffers--;
            }
            DosExitCritSec();
            while (free_buffers == 0)
            {
                ULONG tmp;
                DosWaitEventSem(waitsem,SEM_INDEFINITE_WAIT);
                DosResetEventSem(waitsem,(PULONG)&tmp);
            }
        } else // DART
        {
            if (channels == 1)
            {
                DART_Local.ready_buffers++;
                DART_Local.free_buffers--;
            } else
            {
                if (buffer_index[0].bufferindex == buffer_index[1].bufferindex)
                    DART_Local.ready_buffers++;
                if (buffer_index[0].bufferindex != buffer_index[1].bufferindex)
                    DART_Local.free_buffers--;
            }
            DosExitCritSec();
            while (DART_Local.free_buffers == 0)
            {
                ULONG tmp;
                DosWaitEventSem(waitsem,SEM_INDEFINITE_WAIT);
                DosResetEventSem(waitsem,(PULONG)&tmp);
            }
        }
    }

    buffer_index[channel].index = index;
}

void RTMMMObuffer::write_buffer (int fd){}

void RTMMMObuffer::RTMMMSynchTask(void )
{
    ULONG tmp;

    do
    {
        /* WAIT FOR BUFFER REQUEST */
        DosWaitEventSem(OutputChannel->syncsem,SEM_INDEFINITE_WAIT);
        DosResetEventSem(OutputChannel->syncsem,&tmp);

        if (ready_buffers > 0)
        {
            played ++;
            OutputChannel->buffer = subbuffers[next_play_buffer];
            DosEnterCritSec();
            next_play_buffer += 1;
            next_play_buffer &= buffers_counter_mask;
            ready_buffers--;
            free_buffers  ++;
            DosExitCritSec();
            DosPostEventSem(waitsem);
        } else
        {
            missed ++;
            OutputChannel->buffer = subbuffers[NUMBER_OF_LOCAL_BUFFERS];
        }
    }
    while(1); // Loop for good.....
};

void RTMMMObuffer::SetInputName(char *s){
    if (OutputChannel==NULL)return;
    strncpy(OutputChannel->music_name,s,79);
};


/******************************************************************************/
/*                                                                            */
/*     FileObuffer                                                            */
/*                                                                            */
/******************************************************************************/

FileObuffer::FileObuffer (uint32 number_of_channels)
{
    channels = number_of_channels;
    for (int i = 0; i < (int)number_of_channels; ++i)
        bufferp[i] = buffer + i;
}


void FileObuffer::append (uint32 channel, int16 value)
{
    *bufferp[channel] = value;
    bufferp[channel] += channels;
}


void FileObuffer::write_buffer (int fd)
{
    int length = (int)((char *)bufferp[0] - (char *)buffer), writelength;

    if ((writelength = write (fd, (char *)buffer, length)) != length)
    {
        // buffer has not been written completely
        if (writelength < 0)
        {
            perror ("write");
            exit (1);
        }
        length -= writelength;
        char *buffer_pos = (char *)buffer;
        do
        {
            buffer_pos += writelength;
            if ((writelength = write (fd, buffer_pos, length)) < 0)
            {
                perror ("write");
                exit (1);
            }
        }
        while (length -= writelength);
    }

    for (int i = 0; i < (int)channels; ++i)
        bufferp[i] = buffer + i;
}

/******************************************************************************/
/*                                                                            */
/*     NULLObuffer                                                            */
/*                                                                            */
/******************************************************************************/

NULLObuffer::NULLObuffer (uint32 number_of_channels){}

void NULLObuffer::append (uint32 channel, int16 value){}

void NULLObuffer::write_buffer (int fd){}

/******************************************************************************/
/*                                                                            */
/*     DARTObuffer                                                            */
/*                                                                            */
/******************************************************************************/


DARTObuffer::DARTObuffer (uint32 number_of_channels,uint32 sampling_frequency)
{
    APIRET        ret;
    int           i;
    char          errs[1024];


    memset(&MixSetupParms,'\0',sizeof(MCI_MIXSETUP_PARMS));
    memset(&AmpOpenParms,'\0',sizeof(MCI_AMP_OPEN_PARMS));

    AmpOpenParms.usDeviceID = ( USHORT ) 0;
    AmpOpenParms.pszDeviceType = ( PSZ ) MCI_DEVTYPE_AUDIO_AMPMIX;
    ret = mciSendCommand( 0,MCI_OPEN,MCI_WAIT|MCI_OPEN_TYPE_ID,(void *) &AmpOpenParms,0);
    mciGetErrorString(ret,(PSZ)errs,1023);
    if (ret!=0) cerr << "Error " << errs  << " Opening DART \n";

    MixSetupParms.hwndCallback      = 0;
    MixSetupParms.ulBitsPerSample   = 16;
    MixSetupParms.ulFormatTag       = MCI_WAVE_FORMAT_PCM;
    MixSetupParms.ulSamplesPerSec   = frequencies[sampling_frequency];
    MixSetupParms.ulChannels        = number_of_channels;
    MixSetupParms.ulFormatMode      = MCI_PLAY;
    MixSetupParms.ulDeviceType      = MCI_DEVTYPE_WAVEFORM_AUDIO;
    MixSetupParms.ulMixHandle       = 0; /* RETURN value here */
    MixSetupParms.pmixEvent         = DART_SendBuffers;  /* CallBack routine */
    MixSetupParms.pExtendedInfo     = NULL; /* NO Ext Info */

    ret = mciSendCommand(AmpOpenParms.usDeviceID,MCI_MIXSETUP,MCI_WAIT|MCI_MIXSETUP_INIT,
                         (void *) &MixSetupParms,0);
    mciGetErrorString(ret,(PSZ)errs,1023);
    if (ret!=0) cerr << "Error " << errs  << " Setting up DART \n";

    /* buffer handling */
    BufferParms.ulNumBuffers        = NUMBER_OF_LOCAL_BUFFERS + 2;
    BufferParms.ulBufferSize        = LOCAL_BUFFERS_SIZE * 2;
    BufferParms.pBufList            = (MCI_MIX_BUFFER *)malloc(sizeof(MCI_MIX_BUFFER)*(NUMBER_OF_LOCAL_BUFFERS + 2));

    ret = mciSendCommand(AmpOpenParms.usDeviceID,MCI_BUFFER,MCI_WAIT|MCI_ALLOCATE_MEMORY,
                         (void *) &BufferParms,0);
    mciGetErrorString(ret,(PSZ)errs,1023);
    if (ret!=0) cerr << "Error " << errs  << " Allocating DART Memory \n";

    for (i=0; i < (NUMBER_OF_LOCAL_BUFFERS + 2) ; i++)
    {
        subbuffers[i] = (int16 *)((MCI_MIX_BUFFER *)BufferParms.pBufList)[i].pBuffer;
        memset(subbuffers[i],'\0',LOCAL_BUFFERS_SIZE * 2);
    }

    subbuffer_mask                  = LOCAL_BUFFERS_MASK;
    subbuffer_size                  = LOCAL_BUFFERS_SIZE;
    for (i=0; i<MAXCHANNELS; i++)
    {
        buffer_index[i].bufferindex = 0;
        buffer_index[i].buffer      = (int16 *)subbuffers[0];
        buffer_index[i].index       = i;
    }
    buffer_mask                     = NUMBER_OF_LOCAL_BUFFERS_MASK;
    channels                        = number_of_channels;

    ret = DosCreateEventSem(NULL,&waitsem,0L,0);
    if (ret!=0) cerr << "Error " << ret << " creating event sem \n";

    DART_Local.mix_handle          = MixSetupParms.ulMixHandle;
    DART_Local.MixBuffers          = (MCI_MIX_BUFFER *)BufferParms.pBufList;
    DART_Local.EmptyBuffer         = &((MCI_MIX_BUFFER *)BufferParms.pBufList)[NUMBER_OF_LOCAL_BUFFERS];
    DART_Local.next_play_buffer    = NUMBER_OF_LOCAL_BUFFERS - 2;
    DART_Local.buffers_counter_mask= NUMBER_OF_LOCAL_BUFFERS_MASK;
    DART_Local.ready_buffers       = 2;
    DART_Local.free_buffers        = NUMBER_OF_LOCAL_BUFFERS - 4;
    DART_Local.waitsem             = &waitsem;
    DART_Local.MixSetupParms       = &MixSetupParms;

    /* Kickoff dart */
    MixSetupParms.pmixWrite(MixSetupParms.ulMixHandle,&DART_Local.MixBuffers[NUMBER_OF_LOCAL_BUFFERS-4], 2);

    /* Set priority to high */
    DosSetPriority(PRTYS_THREAD,PRTYC_FOREGROUNDSERVER,0,0);
}

DARTObuffer::~DARTObuffer (void){
    APIRET            ret;
    MCI_GENERIC_PARMS GenericParms;

    while(DART_Local.ready_buffers !=0)
    {
        /* WAIT FOR NEXT PLAYED BUFFER*/
        DosWaitEventSem(waitsem,10000); /* wait 10 second */
    }

    DosCloseEventSem(waitsem);

    ret = mciSendCommand(AmpOpenParms.usDeviceID,MCI_BUFFER,MCI_WAIT|MCI_DEALLOCATE_MEMORY,
                        (void *) &BufferParms,0);
    if (ret!=0) cerr << "Error " << ret << " De-allocating DART Memory \n";

    ret = mciSendCommand(AmpOpenParms.usDeviceID,MCI_CLOSE,MCI_WAIT ,
                        (void *)&GenericParms,0);
    if (ret!=0) cerr << "Error " << ret << " Closing DART  \n";

}

void DARTObuffer::append (uint32 channel, int16 value)
{
    register int index = buffer_index[channel].index;

    buffer_index[channel].buffer[index] = value;

    index+=channels;

    if (index >= subbuffer_size)
    {
        register int bufferindex = buffer_index[channel].bufferindex;

        index &= subbuffer_mask;
        bufferindex++;
        bufferindex &= buffer_mask;

        buffer_index[channel].bufferindex = bufferindex;
        buffer_index[channel].buffer      = subbuffers[bufferindex];

        DosEnterCritSec();
        /* can I release a buffer as done? */
        if (channels == 1)
        {
            DART_Local.ready_buffers++;
            DART_Local.free_buffers--;
        } else
        {
            if (buffer_index[0].bufferindex == buffer_index[1].bufferindex)
                DART_Local.ready_buffers++;
            if (buffer_index[0].bufferindex != buffer_index[1].bufferindex)
                DART_Local.free_buffers--;
        }
        DosExitCritSec();
        while (DART_Local.free_buffers == 0)
        {
            ULONG tmp;
            DosWaitEventSem(waitsem,SEM_INDEFINITE_WAIT);
            DosResetEventSem(waitsem,(PULONG)&tmp);
        }
    }

    buffer_index[channel].index = index;
}

void DARTObuffer::write_buffer (int fd){}



