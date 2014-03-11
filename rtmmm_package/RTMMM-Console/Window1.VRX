/*:VRX         Main
*/
/*  Main
*/
Main:
/*  Process the arguments.
    Get the parent window.
*/
    parse source . calledAs .
    parent = ""
    argCount = arg()
    argOff = 0
    if( calledAs \= "COMMAND" )then do
        if argCount >= 1 then do
            parent = arg(1)
            argCount = argCount - 1
            argOff = 1
        end
    end; else do
        call VROptions 'ImplicitNames'
        call VROptions 'NoEchoQuit'
    end
    InitArgs.0 = argCount
    if( argCount > 0 )then do i = 1 to argCount
        InitArgs.i = arg( i + argOff )
    end
    drop calledAs argCount argOff

/*  Load the windows
*/
    call VRInit
    parse source . . spec
    _VREPrimaryWindowPath = ,
        VRParseFileName( spec, "dpn" ) || ".VRW"
    _VREPrimaryWindow = ,
        VRLoad( parent, _VREPrimaryWindowPath )
    drop parent spec
    if( _VREPrimaryWindow == "" )then do
        call VRMessage "", "Cannot load window:" VRError(), ,
            "Error!"
        _VREReturnValue = 32000
        signal _VRELeaveMain
    end

/*  Process events
*/
    call Init
    signal on halt
    do while( \ VRGet( _VREPrimaryWindow, "Shutdown" ) )
        _VREEvent = VREvent()
        interpret _VREEvent
    end
_VREHalt:
    _VREReturnValue = Fini()
    call VRDestroy _VREPrimaryWindow
_VRELeaveMain:
    call VRFini
exit _VREReturnValue

VRLoadSecondary:
    __vrlsWait = abbrev( 'WAIT', translate(arg(2)), 1 )
    if __vrlsWait then do
        call VRFlush
    end
    __vrlsHWnd = VRLoad( VRWindow(), VRWindowPath(), arg(1) )
    if __vrlsHWnd = '' then signal __vrlsDone
    if __vrlsWait \= 1 then signal __vrlsDone
    call VRSet __vrlsHWnd, 'WindowMode', 'Modal' 
    __vrlsTmp = __vrlsWindows.0
    if( DataType(__vrlsTmp) \= 'NUM' ) then do
        __vrlsTmp = 1
    end
    else do
        __vrlsTmp = __vrlsTmp + 1
    end
    __vrlsWindows.__vrlsTmp = VRWindow( __vrlsHWnd )
    __vrlsWindows.0 = __vrlsTmp
    do while( VRIsValidObject( VRWindow() ) = 1 )
        __vrlsEvent = VREvent()
        interpret __vrlsEvent
    end
    __vrlsTmp = __vrlsWindows.0
    __vrlsWindows.0 = __vrlsTmp - 1
    call VRWindow __vrlsWindows.__vrlsTmp 
    __vrlsHWnd = ''
__vrlsDone:
return __vrlsHWnd

/*:VRX         CH1_LINK_Click
*/
CH1_LINK_Click: 
    call RTMMM_Link 1
return

/*:VRX         CH1_MUTE_Click
*/
CH1_MUTE_Click: 
    call RTMMM_Mute 1
return

/*:VRX         CH2_LINK_Click
*/
CH2_LINK_Click: 
    call RTMMM_Link 2
return

/*:VRX         CH2_MUTE_Click
*/
CH2_MUTE_Click: 
    call RTMMM_Mute 2
return

/*:VRX         CH3_LINK_Click
*/
CH3_LINK_Click: 
    call RTMMM_Link 3
return

/*:VRX         CH3_MUTE_Click
*/
CH3_MUTE_Click: 
    call RTMMM_Mute 3
return

/*:VRX         Fini
*/
Fini:
    window = VRWindow()
    call VRSet window, "Visible", 0
    drop window
return 0

/*:VRX         Halt
*/
Halt:
    signal _VREHalt
return

/*:VRX         Init
*/
Init:
    event_count = 0
    number_of_sliders = 3
    Channels.0 = 0  /* MAX 8 channels */
    Channels.Master = 0 /* Master volume */
    Channels.Busy = 0 /* if the setup is being changed */
    i=1
    do while (i<=8) /* fill remaining channels with 0 */ 
        Channels.i.Active = 0
        Channels.i.Kfilt = 0
        Channels.i.Kright = 0
        Channels.i.Kleft  = 0
        Channels.i.Mute  = 0
        Channels.i.Linked  = 0
        i = i+1
    end

    window = VRWindow()
    call VRMethod window, "CenterWindow"
    call VRSet window, "Visible", 1
    call VRMethod window, "Activate"
    call VRMethod "Application","StartThread","RTMMM_rf"

    Result = Stream("\PIPE\RTMMM_PIPE","C","OPEN");
    if (Result <> "READY:") then do
        return 
    end
    Player = "\PIPE\RTMMM_PIPE"

    drop window
return

/*:VRX         Input1_L_Track
*/
Input1_L_Track: 
    err = RTMMM_Change(1,"L")
return

/*:VRX         Input1_R_Track
*/
Input1_R_Track: 
    err = RTMMM_Change(1,"R")
return

/*:VRX         Input2_L_Track
*/
Input2_L_Track: 
    err = RTMMM_Change(2,"L")
return

/*:VRX         Input2_R_Track
*/
Input2_R_Track: 
    err = RTMMM_Change(2,"R")
return

/*:VRX         Input3_L_Track
*/
Input3_L_Track: 
    err = RTMMM_Change(3,"L")
return

/*:VRX         Input3_R_Track
*/
Input3_R_Track: 
    err = RTMMM_Change(3,"R")
return

/*:VRX         MAIN_VOLUME_Track
*/
MAIN_VOLUME_Track: 
    object = "MAIN_VOLUME"
    value = VRGet(object,"Percentile")
    Channels.Master = value * 327.68
    call RTMMM_SetInfo 
return

/*:VRX         Quit
*/
Quit:
    Result = Stream("\PIPE\RTMMM_PIPE","C","CLOSE");
    window = VRWindow()
    call VRSet window, "Shutdown", 1
    drop window
return

/*:VRX         RTMMM_Change
*/
RTMMM_Change: 
    parse arg channel, position

/*
    sum_other_channels = 2
    if Channels.channel.Linked = 1 then do 
        object = "input"channel"_"position
        value = VRGet(object,"Percentile")
        fvalue = value * 0.01 
        sum_other_channels = sum_other_channels - (Channels.channel.Kright + Channels.channel.Kleft);
        step = (2 * fvalue) - Channels.channel.Kright - Channels.channel.Kleft
        Channels.channel.Kright = value * 0.01
        Channels.channel.Kleft  = value * 0.01
    end
    else do 
        object = "input"channel"_"position
        value = VRGet(object,"Percentile")
        fvalue = value * 0.01
        sum_other_channels = sum_other_channels - (Channels.channel.Kright + Channels.channel.Kleft);
        if position = "R" then step = Channels.channel.Kright
        else                   step = Channels.channel.Kleft
        if position = "R" then Channels.channel.Kright = fvalue             
        else                   Channels.channel.Kleft  = fvalue
        step = fvalue - step
    end

    if  sum_other_channels > 0 then  ratio = 1 - (step / sum_other_channels);
    else ratio = 0

    i=1
    j= 0+channel
    do while (i<=Channels.0)
        if i=j then do
        end
        else do
            Channels.i.Kright = Channels.i.Kright * ratio
            Channels.i.Kleft  = Channels.i.Kleft * ratio
        end
        i=i+1
    end    
    i = 1
*/
    object = "input"channel"_"position
    value = VRGet(object,"Percentile")
    fvalue = value * 327.68 
    if position = "R" then Channels.channel.Kright = fvalue
    else                   Channels.channel.Kleft = fvalue

    if Channels.channel.Linked = 1 then do 
        Channels.channel.Kright = fvalue
        Channels.channel.Kleft  = fvalue
    end
    else do 
        if position = "R" then Channels.channel.Kright = fvalue
        else                   Channels.channel.Kleft = fvalue
    end
   
    call RTMMM_SetInfo 
    call RTMMM_Show 

return 0

/*:VRX         RTMMM_CONSOLE_Close
*/
RTMMM_CONSOLE_Close:
    call Quit
return

/*:VRX         RTMMM_GetInfo
*/
RTMMM_GetInfo: 
   
    call lineout Player,"GetEventNumber ." 
    new_event_count = linein(Player)

    if new_event_count = event_count then return  /* No news good news */
    event_count = new_event_count 

    call lineout Player,"GetClientInfo ." 

    Channels.0 = linein(Player)
    Channels.Master = 0
 
    i=1
    do while (i<=32) /* fill remaining channels with 0 */ 
        Channels.i.Active = 0
        Channels.i.Kfilt  = 0
        Channels.i.Kright = 0
        Channels.i.Kleft  = 0
        i = i+1
    end

    i=1
    do while (i<=Channels.0)
        line = linein(Player)
        parse var line chan name title sampling channels k1 k2 k3 mute master_vol

        chan = chan + 1        
        Channels.Master      = master_vol
        Channels.chan.Mute   = mute
        Channels.chan.Active = 1
        Channels.chan.Kfilt  = k3
        Channels.chan.Kright = k1 
        Channels.chan.Kleft  = k2
        Channels.chan.Name   = name
        Channels.chan.Title  = title

        i=i+1
    end    

    call RTMMM_Show
return

/*:VRX         RTMMM_Link
*/
RTMMM_Link: 
    parse arg channel
    object = "CH"channel"_LINK"
    if Channels.channel.Linked = 0 then do
        Channels.channel.Linked = 1
        err = VRSet(object,"BackColor","Red")
        average = Channels.channel.Kright + Channels.channel.Kleft
        average = average / 2
        Channels.channel.Kright = average
        Channels.channel.Kleft  = average
        call RTMMM_SetInfo
        call RTMMM_Show
    end
    else do
        Channels.channel.Linked = 0
        err = VRSet(object,"BackColor","PaleGray")
    end
return

/*:VRX         RTMMM_Mute
*/
RTMMM_Mute: 
    parse arg channel
    if Channels.channel.Mute = 0 then Channels.channel.Mute = 1
    else Channels.channel.Mute = 0

    call RTMMM_SetInfo
    call RTMMM_Show

return

/*:VRX         RTMMM_Refresh
*/
RTMMM_Refresh: 
    call RTMMM_SetInfo
    call RTMMM_GetInfo
return

/*:VRX         RTMMM_SetInfo
*/
RTMMM_SetInfo: 
 
    i = 1
    do while(i<=32)
        if Channels.i.Active = 1 then do
            channel = i - 1
            master_vol = Channels.Master
            muted = Channels.i.Mute 
            k1 = Channels.i.Kright 
            k2 = Channels.i.Kleft 
            call lineout Player,"SetClientMixing "channel muted master_vol k1 k2 
            j = linein(Player) /* read answer (dummy)*/
        end
        i=i+1
    end


return

/*:VRX         RTMMM_Show
*/
RTMMM_Show: 

    Channels.Busy  = 1 /* avoid recalc of values every time we touch a slider */
    value = trunc ((Channels.Master * 100) / 32768)
    err = VRSet("MAIN_VOLUME","Percentile", value )
 
    i = 1
    do while(i<=number_of_sliders)   
        if Channels.i.Active = 1  then do 
            object = "CH"i"_STATUS" 
            err = VRSet(object,"PicturePath", "LED.ICO" ) 
            object = "input"i"_R"
            fvalue = (Channels.i.Kright * 100) / 32768
            value = trunc(fvalue)
            err = VRSet(object,"Percentile", value  ) 
            object = "input"i"_L"
            fvalue = (Channels.i.Kleft * 100) / 32768
            value = trunc(fvalue)
            err = VRSet(object,"Percentile", value ) 
            object = "CH"i"_MUTE"
            if Channels.i.Mute = 0 then err = VRSet(object,"BackColor","Red")
            else                              err = VRSet(object,"BackColor","PaleGray")
        end
        else do
            object = "CH"i"_STATUS" 
            err = VRSet(object,"PicturePath", "LED2.ICO" ) 
            object = "input"i"_R"
            err = VRSet(object,"Percentile", 0  ) 
            object = "input"i"_L"
            err = VRSet(object,"Percentile", 0 ) 
            object = "CH"i"_MUTE"
            err = VRSet(object,"BackColor","PaleGray")
        end
        i=i+1    
    end
    Channels.Busy = 0

return

