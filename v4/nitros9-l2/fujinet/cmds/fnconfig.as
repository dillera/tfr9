********************************************************************
* fnconfig - terminal configuration manager for FujiNet
*
* Provides a native NitrOS-9 TUI for adapter status, host slots,
* DriveWire disk slots, and WiFi setup.  The display uses only SCF
* terminal control sequences; FujiNet I/O stays behind lib/fuji.as.
*
* Edt/Rev  YYYY/MM/DD  Modified by
* Comment
* ------------------------------------------------------------------
*   1      2026/08/03  Andrew Diller / OpenAI
* Initial Wildbits Jr2 implementation.

stdin               equ       0
stdout              equ       1
* Screen rows for prompts and the key help at the bottom: a Wildbits
* screen has 30 rows, the TFR/911H's terminal (ANSI=1) 24.
                    ifne      ANSI
PromptRow           equ       22
FootRow             equ       23
                    else
PromptRow           equ       24
FootRow             equ       27
                    endc

PAGE$Status         equ       0
PAGE$Hosts          equ       1
PAGE$Devices        equ       2
PAGE$Wifi           equ       3

HOSTSLOTS           equ       8
HOSTSLOTSZ          equ       32
HOSTARRSZ           equ       HOSTSLOTS*HOSTSLOTSZ
DEVSLOTS            equ       4                   DriveWire x0-x3
DEVSLOTSZ           equ       38
DEVFILESZ           equ       36
DEVARRSZ            equ       DEVSLOTS*DEVSLOTSZ

ADPCFGSZ            equ       140
CFGSSID             equ       0
CFGHOST             equ       33
CFGIP               equ       97
CFGGATE             equ       101
CFGMASK             equ       105
CFGDNS              equ       109
CFGVER              equ       125

SSIDNAMESZ          equ       33
SSIDPASSSZ          equ       64
SSIDCFGSZ           equ       SSIDNAMESZ+SSIDPASSSZ
SSIDINFOSZ          equ       34
MAXNETWORKS         equ       10
DIRNAMELEN          equ       40
DIRENTRIES          equ       12

MODE$Read           equ       1
MODE$Write          equ       2
MODE$Mounted        equ       $40

KEY$Up              equ       $0C
KEY$Down            equ       $0A
KEY$Left            equ       $08
KEY$Right           equ       $09
KEY$Enter           equ       C$CR

FUJI$GetWifiEnabled equ       $EA

                    section   __os9
type                equ       Prgrm
lang                equ       Objct
attr                equ       ReEnt
rev                 equ       $00
edition             equ       1
stack               equ       512
                    endsect

                    section   bss
netpath             rmb       1
page                rmb       1
selected            rmb       1
workidx             rmb       1
key                 rmb       1
promptmax           rmb       1
savederr            rmb       1
signal              rmb       1
hostchoice          rmb       1
modechoice          rmb       1
wifistatus          rmb       1
scanCount           rmb       1
dirCount            rmb       1
dirSelected         rmb       1

oldopts             rmb       32
rawopts             rmb       32
                    ifne      ANSI
posbuf              rmb       9                   ESC [ row ; col H NUL
                    else
posbuf              rmb       3
                    endc
charbuf             rmb       3
endptr              rmb       2

request             rmb       261                 largest command used here
hosts               rmb       HOSTARRSZ
devices             rmb       DEVARRSZ
adapter             rmb       ADPCFGSZ+1
ssidcfg             rmb       SSIDCFGSZ
scanlist            rmb       MAXNETWORKS*SSIDINFOSZ
dirlist             rmb       DIRENTRIES*DIRNAMELEN
pathbuf             rmb       256
linebuf             rmb       257
                    endsect

                    section   code

__start             clr       netpath,u
                    lbsr      TermInit
                    lbcs      ExitError
                    lbsr      NOpen
                    lbcs      ExitError
                    sta       netpath,u
                    lbsr      FBReady
                    lbcs      CloseError
                    lbsr      LoadAll
                    lbcs      CloseError
                    clr       page,u
                    clr       selected,u
                    lbsr      MainLoop
                    clrb

CloseError          pshs      b,cc
                    lda       netpath,u
                    beq       RestoreExit
                    os9       I$Close
RestoreExit         lbsr      TermRestore
                    puls      b,cc
ExitError           os9       F$Exit

********************************************************************
* Main application state machine

MainLoop            lbsr      DrawPage
MainKey             lbsr      ReadKey
                    lbcs      MainDone
                    lda       key,u
                    cmpa      #'a
                    blo       MainCommon
                    cmpa      #'z
                    bhi       MainCommon
                    suba      #$20
                    sta       key,u
MainCommon          cmpa      #'Q
                    lbeq      MainDone
                    cmpa      #'I
                    bne       MainPageCheck
                    lbsr      DrawInfo
                    lbsr      ReadKey
                    lbra      MainLoop
MainPageCheck       cmpa      #'1
                    blo       MainRefresh
                    cmpa      #'4
                    bhi       MainRefresh
                    suba      #'1
                    sta       page,u
                    clr       selected,u
                    lbra      MainLoop
MainRefresh         cmpa      #'R
                    bne       MainHelp
                    lbsr      LoadAll
                    lbcs      MainError
                    lbra      MainLoop
MainHelp            cmpa      #'?
                    bne       MainDispatch
                    lbsr      DrawHelp
                    lbsr      ReadKey
                    lbra      MainLoop
MainDispatch        lda       page,u
                    beq       MainKey
                    cmpa      #PAGE$Hosts
                    beq       MainHosts
                    cmpa      #PAGE$Devices
                    beq       MainDevices
                    lbsr      WifiKey
                    lbra      MainAction
MainHosts           lbsr      HostsKey
                    bra       MainAction
MainDevices         lbsr      DevicesKey
MainAction          bcc       MainLoop
MainError           lbsr      ErrorPause
                    lbra      MainLoop
MainDone            rts

DrawPage            lbsr      TClear
                    lbsr      CursorOff
                    clra
                    clrb
                    lbsr      TAt
                    leax      title,pcr
                    lbsr      RevPrint
                    clra
                    ldb       #2
                    lbsr      TAt
                    leax      tabs,pcr
                    lbsr      PrintZ
                    clra
                    ldb       #3
                    lbsr      TAt
                    leax      rule,pcr
                    lbsr      PrintZ
                    lda       page,u
                    beq       DrawStatus
                    cmpa      #PAGE$Hosts
                    lbeq      DrawHosts
                    cmpa      #PAGE$Devices
                    lbeq      DrawDevices
                    lbra      DrawWifi

DrawFooter          clra
                    ldb       #FootRow
                    lbsr      TAt
                    leax      footer,pcr
                    lbsr      PrintZ
                    rts

DrawStatus          clra
                    ldb       #5
                    lbsr      TAt
                    leax      statusHead,pcr
                    lbsr      RevPrint

                    clra
                    ldb       #7
                    lbsr      TAt
                    leax      labelVersion,pcr
                    lbsr      PrintZ
                    leax      adapter+CFGVER,u
                    lbsr      PrintZ

                    clra
                    ldb       #8
                    lbsr      TAt
                    leax      labelHost,pcr
                    lbsr      PrintZ
                    leax      adapter+CFGHOST,u
                    lbsr      PrintZ

                    clra
                    ldb       #9
                    lbsr      TAt
                    leax      labelSSID,pcr
                    lbsr      PrintZ
                    leax      adapter+CFGSSID,u
                    lbsr      PrintZ

                    clra
                    ldb       #10
                    lbsr      TAt
                    leax      labelWifi,pcr
                    lbsr      PrintZ
                    lda       wifistatus,u
                    cmpa      #3
                    bne       StatusDisconnected
                    leax      connected,pcr
                    bra       StatusWifiPrint
StatusDisconnected  leax      disconnected,pcr
StatusWifiPrint     lbsr      PrintZ

                    clra
                    ldb       #12
                    lbsr      TAt
                    leax      labelIP,pcr
                    lbsr      PrintZ
                    leax      adapter+CFGIP,u
                    lbsr      PrintIP
                    clra
                    ldb       #13
                    lbsr      TAt
                    leax      labelGateway,pcr
                    lbsr      PrintZ
                    leax      adapter+CFGGATE,u
                    lbsr      PrintIP
                    clra
                    ldb       #14
                    lbsr      TAt
                    leax      labelMask,pcr
                    lbsr      PrintZ
                    leax      adapter+CFGMASK,u
                    lbsr      PrintIP
                    clra
                    ldb       #15
                    lbsr      TAt
                    leax      labelDNS,pcr
                    lbsr      PrintZ
                    leax      adapter+CFGDNS,u
                    lbsr      PrintIP
                    lbra      DrawFooter

DrawHosts           clra
                    ldb       #5
                    lbsr      TAt
                    leax      hostsHead,pcr
                    lbsr      RevPrint
                    clr       workidx,u
DrawHostLoop        lda       workidx,u
                    cmpa      #HOSTSLOTS
                    bcc       DrawHostsDone
                    clra
                    ldb       workidx,u
                    addb      #7
                    lbsr      TAt
                    lda       workidx,u
                    cmpa      selected,u
                    bne       DrawHostPlain
                    lbsr      ReverseOn
DrawHostPlain       ldb       workidx,u
                    addb      #'0
                    lbsr      PutChar
                    ldb       #':'
                    lbsr      PutChar
                    ldb       #' '
                    lbsr      PutChar
                    leax      hosts,u
                    lda       workidx,u
                    ldb       #HOSTSLOTSZ
                    mul
                    leax      d,x
                    ldb       #HOSTSLOTSZ
                    lbsr      FixedToLine
                    leax      linebuf,u
                    tst       ,x
                    bne       DrawHostName
                    leax      empty,pcr
DrawHostName        lbsr      PrintZ
                    lbsr      ReverseOff
                    lbsr      NewLine
                    inc       workidx,u
                    bra       DrawHostLoop
DrawHostsDone       clra
                    ldb       #18
                    lbsr      TAt
                    leax      hostsKeys,pcr
                    lbsr      PrintZ
                    lbra      DrawFooter

DrawDevices         clra
                    ldb       #5
                    lbsr      TAt
                    leax      devicesHead,pcr
                    lbsr      RevPrint
                    clr       workidx,u
DrawDevLoop         lda       workidx,u
                    cmpa      #DEVSLOTS
                    lbcc      DrawDevsDone
                    clra
                    ldb       workidx,u
                    addb      #7
                    lbsr      TAt
                    lda       workidx,u
                    cmpa      selected,u
                    bne       DrawDevPlain
                    lbsr      ReverseOn
DrawDevPlain        leax      devPrefix,pcr
                    lbsr      PrintZ
                    ldb       workidx,u
                    addb      #'0
                    lbsr      PutChar
                    leax      hostField,pcr
                    lbsr      PrintZ
                    lbsr      DeviceSlotX
                    ldb       ,x
                    cmpb      #$FF
                    beq       DrawNoHost
                    addb      #'0
                    lbsr      PutChar
                    bra       DrawDevMode
DrawNoHost          ldb       #'-
                    lbsr      PutChar
DrawDevMode         leax      modeField,pcr
                    lbsr      PrintZ
                    lbsr      DeviceSlotX
                    lda       1,x
                    bita      #MODE$Write
                    beq       DrawRO
                    leax      modeRW,pcr
                    bra       DrawModeText
DrawRO              leax      modeRO,pcr
DrawModeText        lbsr      PrintZ
                    lbsr      DeviceSlotX
                    lda       1,x
                    bita      #MODE$Mounted
                    beq       DrawIdle
                    leax      mounted,pcr
                    bra       DrawMountText
DrawIdle            leax      idle,pcr
DrawMountText       lbsr      PrintZ
                    leax      fileField,pcr
                    lbsr      PrintZ
                    lbsr      DeviceSlotX
                    leax      2,x
                    ldb       #DEVFILESZ
                    lbsr      FixedToLine
                    leax      linebuf,u
                    tst       ,x
                    bne       DrawDevFile
                    leax      empty,pcr
DrawDevFile         lbsr      PrintZ
                    lbsr      ReverseOff
                    lbsr      NewLine
                    inc       workidx,u
                    lbra      DrawDevLoop
DrawDevsDone        clra
                    ldb       #14
                    lbsr      TAt
                    leax      devicesKeys,pcr
                    lbsr      PrintZ
                    lbra      DrawFooter

DeviceSlotX         leax      devices,u
                    lda       workidx,u
                    ldb       #DEVSLOTSZ
                    mul
                    leax      d,x
                    rts

DrawWifi            clra
                    ldb       #5
                    lbsr      TAt
                    leax      wifiHead,pcr
                    lbsr      RevPrint
                    clra
                    ldb       #7
                    lbsr      TAt
                    leax      labelSSID,pcr
                    lbsr      PrintZ
                    leax      adapter+CFGSSID,u
                    lbsr      PrintZ
                    clra
                    ldb       #8
                    lbsr      TAt
                    leax      labelWifi,pcr
                    lbsr      PrintZ
                    lda       wifistatus,u
                    cmpa      #3
                    bne       DrawWifiDown
                    leax      connected,pcr
                    bra       DrawWifiState
DrawWifiDown        leax      disconnected,pcr
DrawWifiState       lbsr      PrintZ
                    clra
                    ldb       #10
                    lbsr      TAt
                    leax      wifiKeys,pcr
                    lbsr      PrintZ
                    lbra      DrawFooter

DrawHelp            lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      helpText,pcr
                    lbsr      PrintZ
                    rts

DrawInfo            lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      infoHead,pcr
                    lbsr      RevPrint
                    clra
                    ldb       #3
                    lbsr      TAt
                    leax      infoVersion,pcr
                    lbsr      PrintZ
                    clra
                    ldb       #5
                    lbsr      TAt
                    leax      infoAuthor,pcr
                    lbsr      PrintZ
                    clra
                    ldb       #7
                    lbsr      TAt
                    leax      infoFirmware,pcr
                    lbsr      PrintZ
                    leax      adapter+CFGVER,u
                    lbsr      PrintZ
                    clra
                    ldb       #10
                    lbsr      TAt
                    leax      infoReturn,pcr
                    lbsr      PrintZ
                    rts

********************************************************************
* Page input handlers

HostsKey            lda       key,u
                    cmpa      #KEY$Up
                    bne       HostDown
                    lda       selected,u
                    beq       HostHandled
                    dec       selected,u
HostHandled         clrb
                    rts
HostDown            cmpa      #KEY$Down
                    bne       HostEditCheck
                    lda       selected,u
                    cmpa      #HOSTSLOTS-1
                    bhs       HostHandled
                    inc       selected,u
                    bra       HostHandled
HostEditCheck       cmpa      #'E
                    bne       HostMountCheck
                    lbsr      ProtectRootHost
                    bcs       HostReturn
                    lbsr      EditHost
                    rts
HostMountCheck      cmpa      #'M
                    bne       HostUnmountCheck
                    ldb       selected,u
                    lbsr      MountHost
                    bcs       HostReturn
                    lbsr      LoadAll
HostReturn          rts
HostUnmountCheck    cmpa      #'U
                    bne       HostHandled
                    lbsr      ProtectRootHost
                    bcs       HostReturn
                    ldb       selected,u
                    lbsr      UnmountHost
                    bcs       HostReturn
                    lbsr      LoadAll
                    rts

DevicesKey          lda       key,u
                    cmpa      #KEY$Up
                    bne       DevDown
                    lda       selected,u
                    beq       DevHandled
                    dec       selected,u
DevHandled          clrb
                    rts
DevDown             cmpa      #KEY$Down
                    bne       DevEditCheck
                    lda       selected,u
                    cmpa      #DEVSLOTS-1
                    bhs       DevHandled
                    inc       selected,u
                    bra       DevHandled
DevEditCheck        cmpa      #'E
                    bne       DevBrowseCheck
                    lbsr      ProtectRootDevice
                    bcs       DevReturn
                    lbsr      EditDevice
                    rts
DevBrowseCheck      cmpa      #'B
                    bne       DevMountCheck
                    lbsr      ProtectRootDevice
                    bcs       DevReturn
                    lbsr      BrowseDevice
                    rts
DevMountCheck       cmpa      #'M
                    bne       DevUnmountCheck
                    lbsr      ProtectRootDevice
                    bcs       DevReturn
                    lbsr      MountSelectedDevice
                    rts
DevUnmountCheck     cmpa      #'U
                    bne       DevHandled
                    lbsr      ProtectRootDevice
                    bcs       DevReturn
                    ldb       selected,u
                    lbsr      UnmountImage
                    bcs       DevReturn
                    lbsr      LoadDevices
DevReturn           rts

* The DriveWire image is running from /x0.  Mutating that slot, or the host
* that backs it, can remove the live root filesystem from underneath OS-9.
ProtectRootDevice   tst       selected,u
                    bne       ProtectOK
ProtectBusy         comb
                    ldb       #E$DevBsy
                    rts
ProtectRootHost     ldb       devices,u          /x0 host slot
                    cmpb      #$FF
                    beq       ProtectOK
                    cmpb      selected,u
                    beq       ProtectBusy
ProtectOK           andcc     #^Carry
                    rts

WifiKey             lda       key,u
                    cmpa      #'S
                    bne       WifiManualCheck
                    lbsr      WifiScan
                    rts
WifiManualCheck     cmpa      #'E
                    bne       WifiHandled
                    lbsr      WifiManual
                    rts
WifiHandled         clrb
                    rts

********************************************************************
* Host operations

EditHost            leax      askHostName,pcr
                    ldb       #HOSTSLOTSZ-1
                    lbsr      AskLine
                    bcs       EditHostExit
                    leax      hosts,u
                    lda       selected,u
                    ldb       #HOSTSLOTSZ
                    mul
                    leax      d,x
                    ldb       #HOSTSLOTSZ
                    lbsr      ClearX
                    leay      linebuf,u
                    ldb       #HOSTSLOTSZ-1
EditHostCopy        lda       ,y+
                    beq       EditHostWrite
                    sta       ,x+
                    decb
                    bne       EditHostCopy
EditHostWrite       lbsr      WriteHosts
                    bcs       EditHostExit
                    lbsr      LoadAll
EditHostExit        rts

WriteHosts          leax      request,u
                    ldd       #OP_FUJI*256+FUJI$WriteHostSlots
                    std       ,x++
                    leay      hosts,u
                    tfr       y,d
                    addd      #HOSTARRSZ
                    std       endptr,u
WriteHostsLoop      lda       ,y+
                    sta       ,x+
                    cmpy      endptr,u
                    blo       WriteHostsLoop
                    leax      request,u
                    ldy       #2+HOSTARRSZ
                    lbra      FBCmd

MountHost           stb       request+2,u
                    ldd       #OP_FUJI*256+FUJI$MountHost
                    std       request,u
                    leax      request,u
                    ldy       #3
                    lbra      FBCmd

UnmountHost         stb       request+2,u
                    ldd       #OP_FUJI*256+FUJI$UnmountHost
                    std       request,u
                    leax      request,u
                    ldy       #3
                    lbra      FBCmd

********************************************************************
* Device operations

EditDevice          leax      askHostSlot,pcr
                    ldb       #HOSTSLOTS
                    lbsr      AskDigit
                    bcs       EditDeviceExit
                    stb       hostchoice,u
                    lbsr      AskMode
                    bcs       EditDeviceExit
                    stb       modechoice,u
                    leax      askImagePath,pcr
                    ldb       #255
                    lbsr      AskLine
                    bcs       EditDeviceExit
                    lbsr      SetSelectedDevice
                    bcs       EditDeviceExit
                    lbsr      LoadDevices
EditDeviceExit      rts

SetSelectedDevice   leax      request,u
                    ldd       #OP_FUJI*256+FUJI$SetDevicePath
                    std       ,x
                    lda       selected,u
                    sta       2,x
                    lda       hostchoice,u
                    sta       3,x
                    lda       modechoice,u
                    sta       4,x
                    leax      5,x
                    leay      256,x
                    sty       endptr,u
SetDevClear         clr       ,x+
                    cmpx      endptr,u
                    blo       SetDevClear
                    leax      request+5,u
                    leay      linebuf,u
                    ldb       #255
SetDevCopy          lda       ,y+
                    beq       SetDevSend
                    sta       ,x+
                    decb
                    bne       SetDevCopy
SetDevSend          leax      request,u
                    ldy       #261
                    lbra      FBCmd

MountSelectedDevice
                    leax      devices,u
                    lda       selected,u
                    ldb       #DEVSLOTSZ
                    mul
                    leax      d,x
                    ldb       ,x
                    cmpb      #$FF
                    beq       MountDevBad
                    stb       hostchoice,u
                    lda       1,x
                    anda      #MODE$Read+MODE$Write
                    bne       MountDevMode
                    lda       #MODE$Read
MountDevMode        sta       modechoice,u
                    ldb       hostchoice,u
                    lbsr      MountHost
                    bcs       MountDevExit
                    ldd       #OP_FUJI*256+FUJI$MountImage
                    std       request,u
                    lda       selected,u
                    sta       request+2,u
                    lda       modechoice,u
                    sta       request+3,u
                    leax      request,u
                    ldy       #4
                    lbsr      FBCmd
                    bcs       MountDevExit
                    lbsr      LoadDevices
MountDevExit        rts
MountDevBad         comb
                    ldb       #E$IllArg
                    rts

UnmountImage        stb       request+2,u
                    ldd       #OP_FUJI*256+FUJI$UnmountImage
                    std       request,u
                    leax      request,u
                    ldy       #3
                    lbra      FBCmd

********************************************************************
* Host directory browser

BrowseDevice        leax      askHostSlot,pcr
                    ldb       #HOSTSLOTS
                    lbsr      AskDigit
                    lbcs      BrowseExit
                    stb       hostchoice,u
                    lbsr      AskMode
                    lbcs      BrowseExit
                    stb       modechoice,u
                    ldb       hostchoice,u
                    lbsr      MountHost
                    lbcs      BrowseExit
                    leax      pathbuf,u
                    lda       #'/
                    sta       ,x+
                    clr       ,x
                    lbsr      OpenDirectory
                    lbcs      BrowseExit
                    lbsr      ReadDirPage
                    lbcs      BrowseCloseError

BrowseLoop          lbsr      DrawBrowser
                    lbsr      ReadKey
                    lbcs      BrowseCancel
                    lda       key,u
                    cmpa      #'a
                    blo       BrowseKey
                    cmpa      #'z
                    bhi       BrowseKey
                    suba      #$20
BrowseKey           cmpa      #'Q
                    lbeq      BrowseCancel
                    cmpa      #KEY$Up
                    bne       BrowseDown
                    lda       dirSelected,u
                    beq       BrowseLoop
                    dec       dirSelected,u
                    bra       BrowseLoop
BrowseDown          cmpa      #KEY$Down
                    bne       BrowseEnter
                    lda       dirSelected,u
                    inca
                    cmpa      dirCount,u
                    bhs       BrowseLoop
                    sta       dirSelected,u
                    bra       BrowseLoop
BrowseEnter         cmpa      #KEY$Enter
                    bne       BrowseParent
                    tst       dirCount,u
                    beq       BrowseLoop
                    lbsr      BrowserChoose
                    lbcs      BrowseCloseError
                    tstb                          0=entered dir, 1=mounted image
                    bne       BrowseSuccess
                    lbra      BrowseLoop
BrowseParent        cmpa      #'B
                    bne       BrowseNext
                    lbsr      ParentPath
                    lbsr      RestartDirectory
                    lbcs      BrowseCloseError
                    lbra      BrowseLoop
BrowseNext          cmpa      #'N
                    bne       BrowseRestart
                    lbsr      ReadDirPage
                    lbcs      BrowseCloseError
                    lbra      BrowseLoop
BrowseRestart       cmpa      #'R
                    bne       BrowseLoop
                    lbsr      RestartDirectory
                    lbcs      BrowseCloseError
                    lbra      BrowseLoop

BrowseSuccess       lbsr      CloseDirectory
                    bcs       BrowseExit
                    lbsr      LoadDevices
BrowseExit          rts
BrowseCancel        lbsr      CloseDirectory
                    andcc     #^Carry
                    rts
BrowseCloseError    pshs      b,cc
                    lbsr      CloseDirectory
                    puls      b,cc,pc

DrawBrowser         lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      browserHead,pcr
                    lbsr      RevPrint
                    clra
                    ldb       #1
                    lbsr      TAt
                    leax      browserHost,pcr
                    lbsr      PrintZ
                    ldb       hostchoice,u
                    addb      #'0
                    lbsr      PutChar
                    leax      browserPath,pcr
                    lbsr      PrintZ
                    leax      pathbuf,u
                    lbsr      PrintZ
                    clr       workidx,u
BrowserDrawLoop     lda       workidx,u
                    cmpa      dirCount,u
                    bhs       BrowserDrawDone
                    clra
                    ldb       workidx,u
                    addb      #3
                    lbsr      TAt
                    lda       workidx,u
                    cmpa      dirSelected,u
                    bne       BrowserDrawPlain
                    lbsr      ReverseOn
BrowserDrawPlain    leax      dirlist,u
                    lda       workidx,u
                    ldb       #DIRNAMELEN
                    mul
                    leax      d,x
                    ldb       #DIRNAMELEN
                    lbsr      FixedToLine
                    leax      linebuf,u
                    lbsr      PrintZ
                    lbsr      ReverseOff
                    lbsr      NewLine
                    inc       workidx,u
                    bra       BrowserDrawLoop
BrowserDrawDone     clra
                    ldb       #17
                    lbsr      TAt
                    leax      browserKeys,pcr
                    lbsr      PrintZ
                    clra
                    ldb       #18
                    lbsr      TAt
                    leax      browserKeys2,pcr
                    lbsr      PrintZ
                    rts

RestartDirectory    lbsr      CloseDirectory
                    bcs       RestartExit
                    lbsr      OpenDirectory
                    bcs       RestartExit
                    lbsr      ReadDirPage
RestartExit         rts

OpenDirectory       leax      request,u
                    ldd       #OP_FUJI*256+FUJI$OpenDir
                    std       ,x
                    lda       hostchoice,u
                    sta       2,x
                    leax      3,x
                    leay      256,x
                    sty       endptr,u
OpenDirClear        clr       ,x+
                    cmpx      endptr,u
                    blo       OpenDirClear
                    leax      request+3,u
                    leay      pathbuf,u
                    ldb       #255
OpenDirCopy         lda       ,y+
                    beq       OpenDirSend
                    sta       ,x+
                    decb
                    bne       OpenDirCopy
OpenDirSend         leax      request,u
                    ldy       #259
                    lbra      FBCmd

CloseDirectory      ldd       #OP_FUJI*256+FUJI$CloseDir
                    std       request,u
                    leax      request,u
                    ldy       #2
                    lbra      FBCmd

ReadDirPage         clr       dirCount,u
                    clr       dirSelected,u
                    leax      dirlist,u
                    ldy       #DIRENTRIES*DIRNAMELEN
ReadDirClear        clr       ,x+
                    leay      -1,y
                    bne       ReadDirClear
ReadDirLoop         lda       dirCount,u
                    cmpa      #DIRENTRIES
                    bhs       ReadDirDone
                    ldd       #OP_FUJI*256+FUJI$ReadDirEntry
                    std       request,u
                    lda       #DIRNAMELEN
                    sta       request+2,u
                    clr       request+3,u
                    leax      request,u
                    ldy       #4
                    lbsr      FBCmd
                    bcs       ReadDirDone
                    leax      dirlist,u
                    lda       dirCount,u
                    ldb       #DIRNAMELEN
                    mul
                    leax      d,x
                    ldy       #DIRNAMELEN
                    lbsr      FBRead
                    bcs       ReadDirDone
                    ldd       ,x
                    cmpd      #$7F7F
                    beq       ReadDirDone
                    inc       dirCount,u
                    bra       ReadDirLoop
ReadDirDone         rts

* Exit B=0 after entering a directory, B=1 after mounting an image.
BrowserChoose       leax      dirlist,u
                    lda       dirSelected,u
                    ldb       #DIRNAMELEN
                    mul
                    leax      d,x
                    pshs      x
                    ldb       #DIRNAMELEN
BrowserFindEnd      lda       ,x+
                    beq       BrowserFoundEnd
                    decb
                    bne       BrowserFindEnd
BrowserFoundEnd     cmpx      ,s
                    cmpb      #DIRNAMELEN
                    beq       BrowserEmpty
                    lda       -2,x
                    cmpa      #'/
                    bne       BrowserFile
                    puls      y
                    lbsr      AppendDirectory
                    bcs       BrowserChooseExit
                    lbsr      RestartDirectory
                    clrb
BrowserChooseExit   rts
BrowserEmpty        puls      x
                    clrb
                    rts
BrowserFile         puls      y
                    lbsr      BuildImagePath
                    bcs       BrowserChooseExit
                    lbsr      SetSelectedDevice
                    bcs       BrowserChooseExit
                    lbsr      LoadDevices
                    bcs       BrowserChooseExit
                    lbsr      MountSelectedDevice
                    bcs       BrowserChooseExit
                    ldb       #1
                    andcc     #^Carry
                    rts

* Y=directory entry ending in '/'; append it to pathbuf.
AppendDirectory     leax      pathbuf,u
                    ldb       #255
AppendFindEnd       tst       ,x
                    beq       AppendCopy
                    leax      1,x
                    decb
                    bne       AppendFindEnd
                    bra       PathTooLong
AppendCopy          lda       ,y+
                    sta       ,x+
                    beq       AppendOK
                    decb
                    bne       AppendCopy
PathTooLong         comb
                    ldb       #E$IllArg
                    rts
AppendOK            andcc     #^Carry
                    rts

* Y=file entry; build current-directory + name in linebuf.
BuildImagePath      leax      linebuf,u
                    pshs      y
                    leay      pathbuf,u
                    ldb       #255
BuildPathCopy       lda       ,y+
                    beq       BuildNameStart
                    sta       ,x+
                    decb
                    bne       BuildPathCopy
                    puls      y
                    bra       PathTooLong
BuildNameStart      puls      y
BuildNameCopy       lda       ,y+
                    sta       ,x+
                    beq       AppendOK
                    decb
                    bne       BuildNameCopy
                    bra       PathTooLong

ParentPath          leax      pathbuf,u
ParentFindEnd       tst       ,x+
                    bne       ParentFindEnd
                    leax      -1,x
                    leay      pathbuf+1,u
                    sty       endptr,u
                    cmpx      endptr,u
                    bls       ParentDone
                    leax      -2,x
ParentScan          cmpx      endptr,u
                    blo       ParentRoot
                    lda       ,x
                    cmpa      #'/
                    beq       ParentCut
                    leax      -1,x
                    bra       ParentScan
ParentCut           clr       1,x
ParentDone          rts
ParentRoot          leax      pathbuf+1,u
                    clr       ,x
                    rts

********************************************************************
* WiFi operations

WifiManual          leax      askSSID,pcr
                    ldb       #32
                    lbsr      AskLine
                    bcs       WifiManualExit
                    lbsr      ClearSSIDCfg
                    leax      ssidcfg,u
                    leay      linebuf,u
                    ldb       #32
                    lbsr      CopyZMax
                    leax      askPassword,pcr
                    ldb       #63
                    lbsr      AskSecret
                    bcs       WifiManualExit
                    leax      ssidcfg+SSIDNAMESZ,u
                    leay      linebuf,u
                    ldb       #63
                    lbsr      CopyZMax
                    lbsr      ConfirmWifiChange
                    tstb
                    beq       WifiManualExit
                    lbsr      WifiConnect
WifiManualExit      rts

WifiScan            lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      scanning,pcr
                    lbsr      PrintZ
                    ldd       #OP_FUJI*256+FUJI$ScanNetworks
                    std       request,u
                    leax      request,u
                    ldy       #2
                    lbsr      FBCmd
                    lbcs      WifiScanExit
                    leax      scanCount,u
                    ldy       #1
                    lbsr      FBRead
                    lbcs      WifiScanExit
                    lda       scanCount,u
                    cmpa      #MAXNETWORKS
                    bls       WifiCountOK
                    lda       #MAXNETWORKS
                    sta       scanCount,u
WifiCountOK         clr       workidx,u
WifiScanLoop        lda       workidx,u
                    cmpa      scanCount,u
                    bhs       WifiSelect
                    ldd       #OP_FUJI*256+FUJI$GetScanResult
                    std       request,u
                    lda       workidx,u
                    sta       request+2,u
                    leax      request,u
                    ldy       #3
                    lbsr      FBCmd
                    lbcs      WifiScanExit
                    leax      scanlist,u
                    lda       workidx,u
                    ldb       #SSIDINFOSZ
                    mul
                    leax      d,x
                    ldy       #SSIDINFOSZ
                    lbsr      FBRead
                    lbcs      WifiScanExit
                    inc       workidx,u
                    bra       WifiScanLoop
WifiSelect          clr       selected,u
WifiSelectDraw      lbsr      DrawWifiList
                    lbsr      ReadKey
                    bcs       WifiScanCancel
                    lda       key,u
                    cmpa      #'Q
                    beq       WifiScanCancel
                    cmpa      #'q
                    beq       WifiScanCancel
                    cmpa      #KEY$Up
                    bne       WifiSelectDown
                    lda       selected,u
                    beq       WifiSelectDraw
                    dec       selected,u
                    bra       WifiSelectDraw
WifiSelectDown      cmpa      #KEY$Down
                    bne       WifiSelectEnter
                    lda       selected,u
                    inca
                    cmpa      scanCount,u
                    bhs       WifiSelectDraw
                    sta       selected,u
                    bra       WifiSelectDraw
WifiSelectEnter     cmpa      #KEY$Enter
                    bne       WifiSelectDraw
                    tst       scanCount,u
                    beq       WifiScanCancel
                    lbsr      ClearSSIDCfg
                    leax      scanlist,u
                    lda       selected,u
                    ldb       #SSIDINFOSZ
                    mul
                    leay      d,x
                    leax      ssidcfg,u
                    ldb       #32
                    lbsr      CopyZMax
                    leax      askPassword,pcr
                    ldb       #63
                    lbsr      AskSecret
                    bcs       WifiScanCancel
                    leax      ssidcfg+SSIDNAMESZ,u
                    leay      linebuf,u
                    ldb       #63
                    lbsr      CopyZMax
                    lbsr      ConfirmWifiChange
                    tstb
                    beq       WifiScanCancel
                    lbsr      WifiConnect
WifiScanCancel      clr       selected,u
WifiScanExit        rts

DrawWifiList        lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      wifiListHead,pcr
                    lbsr      RevPrint
                    clr       workidx,u
WifiDrawLoop        lda       workidx,u
                    cmpa      scanCount,u
                    bhs       WifiDrawDone
                    clra
                    ldb       workidx,u
                    addb      #3
                    lbsr      TAt
                    lda       workidx,u
                    cmpa      selected,u
                    bne       WifiDrawPlain
                    lbsr      ReverseOn
WifiDrawPlain       leax      scanlist,u
                    lda       workidx,u
                    ldb       #SSIDINFOSZ
                    mul
                    leax      d,x
                    lbsr      PrintZ
                    lbsr      ReverseOff
                    lbsr      NewLine
                    inc       workidx,u
                    bra       WifiDrawLoop
WifiDrawDone        clra
                    ldb       #16
                    lbsr      TAt
                    leax      wifiListKeys,pcr
                    lbsr      PrintZ
                    rts

ClearSSIDCfg        leax      ssidcfg,u
                    ldb       #SSIDCFGSZ
                    lbra      ClearX

WifiConnect         lbsr      TClear
                    clra
                    clrb
                    lbsr      TAt
                    leax      connecting,pcr
                    lbsr      PrintZ
                    leax      request,u
                    ldd       #OP_FUJI*256+FUJI$SetSSID
                    std       ,x++
                    leay      ssidcfg,u
                    ldb       #SSIDCFGSZ
WifiConnectCopy     lda       ,y+
                    sta       ,x+
                    decb
                    bne       WifiConnectCopy
                    leax      request,u
                    ldy       #2+SSIDCFGSZ
                    lbsr      FBCmd
                    bcs       WifiConnectExit
                    lbsr      LoadStatus
WifiConnectExit     rts

ConfirmWifiChange   leax      wifiWarning,pcr
                    lbsr      PromptDraw
                    lbsr      ReadKey
                    bcs       ConfirmWifiNo
                    ldb       key,u
                    andb      #$DF
                    cmpb      #'Y
                    beq       ConfirmWifiYes
ConfirmWifiNo       clrb
                    andcc     #^Carry
                    rts
ConfirmWifiYes      ldb       #1
                    andcc     #^Carry
                    rts

********************************************************************
* FujiNet data loading

LoadAll             lbsr      LoadStatus
                    bcs       LoadExit
                    lbsr      LoadHosts
                    bcs       LoadExit
                    lbsr      LoadDevices
LoadExit            rts

LoadStatus          ldd       #OP_FUJI*256+FUJI$GetAdapterCfg
                    std       request,u
                    leax      request,u
                    ldy       #2
                    lbsr      FBCmd
                    bcs       LoadStatusExit
                    leax      adapter,u
                    ldy       #ADPCFGSZ
                    lbsr      FBRead
                    bcs       LoadStatusExit
                    clr       adapter+ADPCFGSZ,u
                    ldd       #OP_FUJI*256+FUJI$GetWifiStatus
                    std       request,u
                    leax      request,u
                    ldy       #2
                    lbsr      FBCmd
                    bcs       LoadStatusExit
                    leax      wifistatus,u
                    ldy       #1
                    lbsr      FBRead
LoadStatusExit      rts

LoadHosts           ldd       #OP_FUJI*256+FUJI$ReadHostSlots
                    std       request,u
                    leax      request,u
                    ldy       #2
                    lbsr      FBCmd
                    bcs       LoadHostsExit
                    leax      hosts,u
                    ldy       #HOSTARRSZ
                    lbsr      FBRead
LoadHostsExit       rts

LoadDevices         ldd       #OP_FUJI*256+FUJI$ReadDevSlots
                    std       request,u
                    leax      request,u
                    ldy       #2
                    lbsr      FBCmd
                    bcs       LoadDevicesExit
                    leax      devices,u
                    ldy       #DEVARRSZ
                    lbsr      FBRead
LoadDevicesExit     rts

********************************************************************
* Terminal input

TermInit            lda       #stdin
                    clr       signal,u
                    leax      SignalHandler,pcr
                    os9       F$Icpt
                    bcs       TermExit
                    lda       #stdin
                    ldb       #SS.Opt
                    leax      oldopts,u
                    os9       I$GetStt
                    bcs       TermExit
                    leax      oldopts,u
                    leay      rawopts,u
                    ldb       #32
TermCopyOpts        lda       ,x+
                    sta       ,y+
                    decb
                    bne       TermCopyOpts
                    leax      rawopts,u
                    clr       (PD.EKO-PD.OPT),x
                    clr       (PD.ALF-PD.OPT),x
                    clr       (PD.PAU-PD.OPT),x
                    lda       #stdin
                    ldb       #SS.Opt
                    os9       I$SetStt
                    bcs       TermExit
                    lbsr      CursorOff
TermExit            rts

TermRaw             lda       #stdin
                    ldb       #SS.Opt
                    leax      rawopts,u
                    os9       I$SetStt
                    rts

TermCooked          lda       #stdin
                    ldb       #SS.Opt
                    leax      oldopts,u
                    os9       I$SetStt
                    rts

TermRestore         pshs      a,b,x,y,cc
                    lbsr      CursorOn
                    lbsr      TClear
                    lda       #stdin
                    ldb       #SS.Opt
                    leax      oldopts,u
                    os9       I$SetStt
                    puls      a,b,x,y,cc,pc

ReadKey             lda       #stdin
                    clr       signal,u
                    leax      key,u
                    ldy       #1
                    os9       I$Read
                    rts

SignalHandler       stb       signal,u
                    rti

AskLine             stb       promptmax,u
                    lbsr      PromptDraw
                    leax      linebuf,u
                    ldy       #257
AskLineClear        clr       ,x+
                    leay      -1,y
                    bne       AskLineClear
                    lbsr      TermCooked
                    bcs       AskLineExit
                    leax      linebuf,u
                    clra
                    ldb       promptmax,u
                    tfr       d,y
                    leay      1,y
                    lda       #stdin
                    os9       I$ReadLn
                    pshs      b,cc
                    lbsr      TermRaw
                    puls      b,cc
                    bcs       AskLineExit
                    leax      linebuf,u
AskLineTerm         lda       ,x
                    beq       AskLineOK
                    cmpa      #C$CR
                    beq       AskLineZero
                    leax      1,x
                    bra       AskLineTerm
AskLineZero         clr       ,x
AskLineOK           andcc     #^Carry
AskLineExit         rts

AskSecret           stb       promptmax,u
                    lbsr      PromptDraw
                    leax      linebuf,u
                    clrb
SecretLoop          pshs      b,x
                    lbsr      ReadKey
                    puls      b,x
                    bcs       SecretExit
                    lda       key,u
                    cmpa      #KEY$Enter
                    beq       SecretDone
                    cmpa      #KEY$Left
                    bne       SecretChar
                    tstb
                    beq       SecretLoop
                    decb
                    leax      -1,x
                    leay      eraseChar,pcr
                    pshs      b,x
                    tfr       y,x
                    lbsr      PrintZ
                    puls      b,x
                    bra       SecretLoop
SecretChar          cmpa      #$20
                    blo       SecretLoop
                    cmpb      promptmax,u
                    bhs       SecretLoop
                    sta       ,x+
                    incb
                    pshs      b,x
                    ldb       #'*'
                    lbsr      PutChar
                    puls      b,x
                    bra       SecretLoop
SecretDone          clr       ,x
                    andcc     #^Carry
SecretExit          rts

AskDigit            stb       promptmax,u
                    lbsr      PromptDraw
AskDigitLoop        lbsr      ReadKey
                    bcs       AskDigitExit
                    ldb       key,u
                    cmpb      #'0
                    blo       AskDigitLoop
                    subb      #'0
                    cmpb      promptmax,u
                    bhs       AskDigitLoop
                    andcc     #^Carry
AskDigitExit        rts

AskMode             leax      askMode,pcr
                    lbsr      PromptDraw
AskModeLoop         lbsr      ReadKey
                    bcs       AskModeExit
                    ldb       key,u
                    andb      #$DF
                    cmpb      #'R
                    beq       AskModeRead
                    cmpb      #'W
                    bne       AskModeLoop
                    ldb       #MODE$Write
                    bra       AskModeOK
AskModeRead         ldb       #MODE$Read
AskModeOK           andcc     #^Carry
AskModeExit         rts

PromptDraw          pshs      x
                    clra
                    ldb       #PromptRow
                    lbsr      TAt
                    lbsr      ClearLine
                    clra
                    ldb       #PromptRow
                    lbsr      TAt
                    puls      x
                    lbsr      PrintZ
                    lbsr      CursorOn
                    rts

********************************************************************
* Terminal output

                    ifeq      ANSI
TClear              leax      ctlClear,pcr
                    lbra      WriteCtl1
ClearLine           leax      ctlClearLine,pcr
                    lbra      WriteCtl1
CursorOff           leax      ctlCursorOff,pcr
                    lbra      WriteCtl2
CursorOn            leax      ctlCursorOn,pcr
                    lbra      WriteCtl2
ReverseOn           leax      ctlReverseOn,pcr
                    lbra      WriteCtl2
ReverseOff          leax      ctlReverseOff,pcr
                    lbra      WriteCtl2
                    else
                    use       fnansi.as           ANSI terminal (the TFR/911H)
                    endc
NewLine             leax      ctlNewLine,pcr
                    lbra      WriteCtl2
WriteCtl1           ldy       #1
                    bra       WriteOut
WriteCtl2           ldy       #2
WriteOut            lda       #stdout
                    os9       I$Write
                    rts

                    ifeq      ANSI
TAt                 adda      #$20
                    addb      #$20
                    std       posbuf+1,u
                    lda       #$02
                    sta       posbuf,u
                    leax      posbuf,u
                    ldy       #3
                    bra       WriteOut
                    endc

PutChar             stb       charbuf,u
                    leax      charbuf,u
                    ldy       #1
                    bra       WriteOut

PrintZ              pshs      x
                    ldy       #0
PrintZCount         tst       ,x+
                    beq       PrintZWrite
                    leay      1,y
                    bra       PrintZCount
PrintZWrite         puls      x
                    cmpy      #0
                    beq       PrintZExit
                    lbsr      WriteOut
PrintZExit          rts

RevPrint            pshs      x
                    lbsr      ReverseOn
                    puls      x
                    lbsr      PrintZ
                    lbra      ReverseOff

PrintIP             pshs      x
                    ldb       #4
PrintIPLoop         pshs      b,x
                    clra
                    ldb       ,x
                    lbsr      PRINT_DEC
                    puls      b,x
                    leax      1,x
                    decb
                    beq       PrintIPDone
                    pshs      b,x
                    ldb       #'.
                    lbsr      PutChar
                    puls      b,x
                    bra       PrintIPLoop
PrintIPDone         puls      x,pc

ErrorPause          stb       savederr,u
                    clra
                    ldb       #PromptRow
                    lbsr      TAt
                    lbsr      ClearLine
                    clra
                    ldb       #PromptRow
                    lbsr      TAt
                    leax      errorLabel,pcr
                    lbsr      PrintZ
                    clra
                    ldb       savederr,u
                    lbsr      PRINT_DEC
                    ldb       savederr,u
                    cmpb      #E$DevBsy
                    bne       ErrorPauseKey
                    leax      rootProtected,pcr
                    lbsr      PrintZ
ErrorPauseKey
                    leax      pressKey,pcr
                    lbsr      PrintZ
                    lbsr      ReadKey
                    andcc     #^Carry
                    rts

********************************************************************
* Small buffer helpers

ClearX              clr       ,x+
                    decb
                    bne       ClearX
                    rts

* X=destination, Y=source, B=maximum bytes; always terminates destination.
CopyZMax            lda       ,y+
                    beq       CopyZDone
                    sta       ,x+
                    decb
                    bne       CopyZMax
CopyZDone           clr       ,x
                    rts

* X=fixed-width source, B=width; copies to linebuf and appends NUL.
FixedToLine         leay      linebuf,u
FixedCopy           lda       ,x+
                    sta       ,y+
                    decb
                    bne       FixedCopy
                    clr       ,y
                    rts

********************************************************************
* Terminal strings and control sequences

ctlClear            fcb       $0C
ctlClearLine        fcb       $03
ctlCursorOff        fcb       $05,$20
ctlCursorOn         fcb       $05,$21
ctlReverseOn        fcb       $1F,$20
ctlReverseOff       fcb       $1F,$21
ctlNewLine          fcb       $0D,$0A
eraseChar           fcb       $08,' ',$08,0

title               fcn       " FujiNet CONFIG for NitrOS-9 "
tabs                fcn       "[1] Status  [2] Hosts  [3] Devices  [4] WiFi  [I] Info  [?] Help  [Q] Quit"
rule                fcn       "-------------------------------------------------------------------------------"
footer              fcn       "R refresh   1-4 change page   I info   ? help   Q quit"

statusHead          fcn       " Adapter Status "
hostsHead           fcn       " Host Slots "
devicesHead         fcn       " DriveWire Device Slots "
wifiHead            fcn       " WiFi Configuration "
labelVersion        fcn       "Firmware: "
labelHost           fcn       "Hostname: "
labelSSID           fcn       "SSID:     "
labelWifi           fcn       "WiFi:     "
labelIP             fcn       "IP:       "
labelGateway        fcn       "Gateway:  "
labelMask           fcn       "Netmask:  "
labelDNS            fcn       "DNS:      "
connected           fcn       "connected"
disconnected        fcn       "not connected"
empty               fcn       "<empty>"

hostsKeys           fcn       "Up/Down select   E edit   M mount   U unmount"
devicesKeys         fcn       "Up/Down select   B browse   E edit path   M mount   U eject"
wifiKeys            fcn       "S scan/select network   E enter SSID manually"

devPrefix           fcn       "/x"
hostField           fcn       "  host="
modeField           fcn       "  "
modeRO              fcn       "RO "
modeRW              fcn       "RW "
mounted             fcn       "mounted "
idle                fcn       "idle    "
fileField           fcn       "  "

askHostName         fcn       "Host name (blank clears): "
askHostSlot         fcn       "Host slot [0-7]: "
askMode             fcn       "Mode [R]ead-only or [W]rite: "
askImagePath        fcn       "Image path (for example /disk.dsk): "
askSSID             fcn       "WiFi SSID: "
askPassword         fcn       "WiFi password: "
wifiWarning         fcn       "Changing WiFi may interrupt /x0. Continue [Y/N]? "

scanning            fcn       "Scanning for WiFi networks..."
connecting          fcn       "Connecting FujiNet to WiFi..."
wifiListHead        fcn       " Select WiFi Network "
wifiListKeys        fcn       "Up/Down select   Enter connect   Q cancel"
browserHead         fcn       " Select Image from FujiNet Host "
browserHost         fcn       "Host "
browserPath         fcn       "   Path "
browserKeys         fcn       "Up/Down select   Enter open/select   B parent   N next page"
browserKeys2        fcn       "R restart directory   Q cancel"
errorLabel          fcn       "Error #"
rootProtected       fcn       " - /x0 is the live root filesystem"
pressKey            fcn       " - press any key"

infoHead            fcn       " About FujiNet CONFIG "
infoVersion         fcn       "Version 1.0"
infoAuthor          fcn       "Written for FujiNet by Diller"
infoFirmware        fcn       "FujiNet firmware: "
infoReturn          fcn       "Press any key to return."

helpText            fcc       "FujiNet CONFIG for NitrOS-9"
                    fcb       C$CR,C$CR
                    fcc       "Status: adapter firmware, network identity, and IP settings."
                    fcb       C$CR
                    fcc       "Hosts: edit and mount any of FujiNet's eight host slots."
                    fcb       C$CR
                    fcc       "Devices: configure, mount, or eject DriveWire /x0-/x3."
                    fcb       C$CR
                    fcc       "WiFi: scan for an access point or enter an SSID manually."
                    fcb       C$CR,C$CR
                    fcc       "Arrow keys move within lists.  R refreshes data from FujiNet."
                    fcb       C$CR
                    fcc       "I shows application and FujiNet firmware version information."
                    fcb       C$CR
                    fcc       "All changes are saved by FujiNet when the command succeeds."
                    fcb       C$CR,C$CR
                    fcn       "Press any key to return."

                    endsect
