/*:VRX         Main
*/
Main:
    signal on halt
    call RxFuncAdd 'SysLoadFuncs','Rexxutil','SysLoadFuncs'
    call SysLoadfuncs
    call RefreshAction
exit

RefreshAction: procedure

do forever
    call SysSleep 1
    call VRMethod "Application","PostQueue",0,0,"call RTMMM_Refresh","Null",0
end

/*:VRX         Halt
*/
Halt:

exit

