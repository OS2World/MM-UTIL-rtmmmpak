#define  INCL_OS2MM                 /* required for MCI and MMIO headers   */
#define  INCL_DOS

#include <os2.h>
#include <os2me.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <signal.h>
#include "rtmmm.h"

#define NUMBER_OF_LOCAL_BUFFERS     2
#define LOCAL_FREQUENCY         40000
#define LOCAL_DATA_SIZE            16
#define LOCAL_N_CHANNELS            2


RTMMM_InputChannel_type *OutputChannel;
RTMMM_Definition_type   *MixerInfo;
HQUEUE                   server_queue;


void exitfunc(){}

void breakfunc(int signo){
    RTMMM_Error(
        DosWriteQueue(server_queue,RTMMM_DISCONNECT,
            10,OutputChannel,0)
    ,ERR_DOS,"Sending message to Server");
    printf("Hello!!\n");
    exit(99);
}

void main(){
    PSZ                SharedMemName = RTMMM_MEMORY;
    PSZ                LocalSharedMemName = "\\SHAREMEM\\RTMMM_TEST1";
    PSZ                MessageQueueName = RTMMM_QUEUE;
    int                SharMemSize;
    int                SamplesPerBuffer;
    int                frequency_ratio;
    int                i,tmp;
    void               *buffers[NUMBER_OF_LOCAL_BUFFERS];
    PID                server_id;
    float              k_filt;
    float              gain;
    
    struct sigaction   sa;

    sa.sa_handler = breakfunc;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,&sa,NULL);


    /* log to server memory */
    RTMMM_Error(
        DosGetNamedSharedMem((void *)&MixerInfo,SharedMemName,PAG_READ)
    ,ERR_DOS,"Linking to Server Shared memory");

    printf("Buffer Size          %8i\n",MixerInfo->buffers_size);
    printf("Number of Buffers    %8i\n",MixerInfo->buffers_number);
    printf("Number of Inputs     %8i\n",MixerInfo->inputs_number);
    printf("First Input address  %8x\n",MixerInfo->inputs);
    printf("Max Data Size        %8i\n",MixerInfo->MixSetupParms.ulBitsPerSample);
    printf("Sampling rate max    %8i\n",MixerInfo->MixSetupParms.ulSamplesPerSec);
    printf("Number of Channels   %8i\n",MixerInfo->MixSetupParms.ulChannels);

    /* create own memory */

    k_filt = (float)LOCAL_FREQUENCY  /  (float)MixerInfo->MixSetupParms.ulSamplesPerSec;
    gain   = 0.5;
    k_filt = 0.2;

    frequency_ratio = (LOCAL_N_CHANNELS * 256 * LOCAL_FREQUENCY) / MixerInfo->MixSetupParms.ulSamplesPerSec;

    SamplesPerBuffer = MixerInfo->buffers_size / MixerInfo->MixSetupParms.ulChannels;
    if (MixerInfo->MixSetupParms.ulBitsPerSample == 16) SamplesPerBuffer /= 2;
    SamplesPerBuffer *= frequency_ratio;
    SamplesPerBuffer >>= 8;

    printf(" !!freq ratio %i \n",frequency_ratio);
    printf(" !!samples    %i \n",SamplesPerBuffer);

    SharMemSize  = sizeof(RTMMM_InputChannel_type) +
        NUMBER_OF_LOCAL_BUFFERS * SamplesPerBuffer * LOCAL_N_CHANNELS * 2; /* 2 => 16 bit */
    printf ("Allocating shared block of %i bytes \n",SharMemSize);
    RTMMM_Error(
        DosAllocSharedMem((void *)&OutputChannel,NULL,
            SharMemSize,PAG_COMMIT | PAG_WRITE | OBJ_GETTABLE | OBJ_GIVEABLE)
    ,ERR_DOS,"Local Shared Memory Allocation");

    OutputChannel->frequency_ratio = frequency_ratio;
    OutputChannel->size            = LOCAL_DATA_SIZE;
    OutputChannel->channels        = LOCAL_N_CHANNELS;
    OutputChannel->buffer          = NULL;
    OutputChannel->mute            = 1;
    OutputChannel->k[0]            = (int)(32768.0 * k_filt * gain);
    OutputChannel->k[1]            = (int)(32768.0 * k_filt * gain);
    OutputChannel->k[2]            = (int)(32768.0 * (1-k_filt));
    OutputChannel->old_output[0]   = 0;
    OutputChannel->old_output[1]   = 0;

    for (i=0;i<NUMBER_OF_LOCAL_BUFFERS;i++)
        buffers[i] = (void *) ((int)OutputChannel +
            sizeof(RTMMM_InputChannel_type) + SamplesPerBuffer * i * LOCAL_N_CHANNELS * 2);


    /* Prepare semaphore */
    RTMMM_Error(
        DosCreateEventSem(NULL,&OutputChannel->syncsem,DC_SEM_SHARED,0)
    ,ERR_DOS,"Creating Local Sync Semaphore");
    printf("Client semaphore = %x ->%x\n",&OutputChannel->syncsem,OutputChannel->syncsem);

    RTMMM_Error(
        DosOpenQueue(&server_id,&server_queue,MessageQueueName)
    ,ERR_DOS,"Opening Server message queue");

    printf("Server Queue found : server pid 0x%x - %i\n",(int)server_id,(int)server_id);

    printf("Address of data sent %x\n",OutputChannel);

    RTMMM_Error(
        DosWriteQueue(server_queue,RTMMM_CONNECT,
            1,OutputChannel,0)
    ,ERR_DOS,"Sending message to Server");

    /* create buffer and play it 100 times */
    {
        struct {short l; short r;} *buffer;
        int j;
        float ratio;

        buffer = buffers[0];
        printf("Buffer = %x\n",buffer);
        ratio = 100.0 * 6.2830 / SamplesPerBuffer;
        for (i=0;i<SamplesPerBuffer;i++){
            buffer[i].l = (short) (10000 * sin (i*ratio));
            buffer[i].r = (short) (10000 * cos (i*ratio));
        }

        OutputChannel->buffer = buffers[0];
        DosSetPriority(PRTYS_THREAD,PRTYC_TIMECRITICAL,-2,0);
        for (i=0;i<5000;i++){
            printf("Sending buffer %i\015",i);
            RTMMM_Error(
                DosWaitEventSem(OutputChannel->syncsem,SEM_INDEFINITE_WAIT)
            ,ERR_DOS,"Waiting for sync from server");

            RTMMM_Error(
                DosResetEventSem(OutputChannel->syncsem,(PULONG)&tmp)
            ,ERR_DOS,"Resetting sync semaphore");
            OutputChannel->buffer = buffers[0];
        }
        DosSetPriority(PRTYS_THREAD,PRTYC_REGULAR,0,0);
    }


    RTMMM_Error(
        DosWriteQueue(server_queue,RTMMM_DISCONNECT,
            10,OutputChannel,0)
    ,ERR_DOS,"Sending message to Server");

    printf("Connected to the server \n");



}
