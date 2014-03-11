#define  INCL_OS2MM                 /* required for MCI and MMIO headers   */
#define  INCL_DOS

#include <os2.h>
#include <os2me.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include "rtmmm.h"

RTMMM_InputChannel_type *OutputChannel;
RTMMM_Definition_type   *MixerInfo;

void exitfunc(){}

void main(){
    PSZ                SharedMemName = RTMMM_MEMORY;
    PSZ                MessageQueueName = RTMMM_QUEUE;

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




}
