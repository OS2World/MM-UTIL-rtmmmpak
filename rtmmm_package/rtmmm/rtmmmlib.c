#define  INCL_OS2MM                 /* required for MCI and MMIO headers   */
#define  INCL_DOS

#include <os2.h>
#include <os2me.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include "rtmmm.h"


void RTMMM_Error(int error,int err_class,char *extra){
    char  s[80];
    if (error == 0) return;
    switch(err_class){
        case ERR_MCI:
            mciGetErrorString(error, (PSZ)s,80);
        break;
        case ERR_DOS:
            sprintf(s," Dos Error %i exiting \n",error);
        break;
    }
    printf("Error :%s\n ",s);
    printf("Info> %s\n ",extra);
    exitfunc();
    exit();
}
