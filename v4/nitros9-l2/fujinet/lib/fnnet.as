********************************************************************
* fnnet - FujiNet network-device transactions for DriveWire
*
* Supplies the small HTTPS/JSON subset used by native NitrOS-9
* applications.  Requests use OP_FUJINET through scdwv's atomic
* SS.Fuji transfer so the UART poller cannot consume raw replies.
*
* Edt/Rev  YYYY/MM/DD  Modified by
* Comment
* ------------------------------------------------------------------
*   1      2026/08/03  Andrew Diller / OpenAI
* Initial DriveWire network and JSON client.

FN$Unit             equ       1
FN$OpenRead         equ       4
FN$TransNone        equ       0
FN$JsonMode         equ       1
FN$Success          equ       1
* HTTPS responses from Open-Meteo can take several seconds on the ESP32.
* Leave a bounded ten-second window before requesting the raw parse status.
FN$ParseWait        equ       600                 ten seconds at 60 Hz
FN$StartupSettle    equ       60                  one second after opening /N1

FN$Ready            equ       $00
FN$Response         equ       $01
FN$Error            equ       $02
FN$Open             equ       'O
FN$Close            equ       'C
FN$Read             equ       'R
FN$Status           equ       'S
FN$Parse            equ       'P
FN$Query            equ       'Q
FN$ChannelMode      equ       $FC

FN$ReqSize          equ       263                 prefix + frame + 256 data

                    section   bss
fnpath              rmb       1
fnout               rmb       2
fndest              rmb       2
fnavail             rmb       2
* Keep a query's advertised value length separate from fnavail: FNGetResponse
* owns fnavail while it builds the transport's raw-response transaction.
fnreadlen           rmb       2
fncapacity          rmb       1
fncopy              rmb       1
fnerror             rmb       1
fnstatus            rmb       4                   avail:2, connected, error
* Retain the most recent SS.Fuji arguments for on-screen hardware diagnosis.
fnxferlen           rmb       2
fnxfererr           rmb       1
fnreq               rmb       FN$ReqSize
fnresp              rmb       257
                    endsect

                    section   code

* Open the NitrOS-9 transport path and confirm FujiNet is responding.
* Exit: A=path and carry clear, or B=OS-9 error and carry set.
FNNetInit           lbsr      NOpen
                    bcs       FNInitExit
                    sta       fnpath,u
* Give FujiNet's network endpoint a short startup window without issuing the
* legacy OP_FUJI/Ready request.  Its raw one-byte reply can otherwise shift
* the following OP_FUJINET response stream on the RS-232 transport.
* F$Sleep may alter U; retain the application's data-area register because
* every following request is built from U-relative BSS.
                    pshs      u
                    ldx       #FN$StartupSettle
                    os9       F$Sleep
                    puls      u
FNInitExit          rts

* Entry: X=N: URL, NUL terminated (255 characters maximum).
FNNetOpen           pshs      x
                    lbsr      FNClearReq
                    leax      fnreq,u
                    ldd       #0
                    std       ,x
                    lda       #OP_FUJINET
                    sta       2,x
                    lda       #FN$Unit
                    sta       3,x
                    lda       #FN$Open
                    sta       4,x
                    lda       #FN$OpenRead
                    sta       5,x
                    clr       6,x
                    puls      x
                    leay      fnreq+7,u
                    lbsr      FNCopy255
                    bcs       FNOpenExit
                    leax      fnreq,u
                    ldy       #FN$ReqSize
                    lbsr      FNSendCheck
FNOpenExit          rts

FNNetClose          lbsr      FNBuildShort
                    lda       #FN$Close
                    sta       fnreq+4,u
                    leax      fnreq,u
                    ldy       #7
                    lbra      FNSendCheck

* Select FujiNet JSON mode and parse the open HTTP response.
FNNetParse          lbsr      FNBuildShort
                    lda       #FN$ChannelMode
                    sta       fnreq+4,u
                    lda       #FN$JsonMode
                    sta       fnreq+5,u
                    leax      fnreq,u
                    ldy       #7
                    lbsr      FNSendCheck
                    bcs       FNParseExit
                    lbsr      FNBuildShort
                    lda       #FN$Parse
                    sta       fnreq+4,u
                    leax      fnreq,u
                    ldy       #7
                    lbsr      FNSend
                    bcs       FNParseExit
* Parse may perform a complete HTTP(S) transaction.  Let the ESP finish before
* issuing a JSON query.  Do not request the one-byte parse result here: RS-232
* FujiNet completes the HTTP transaction but can omit that asynchronous byte.
* The first JSON query validates the parsed document through its normal
* status/read exchange and reports a useful error if parsing did not succeed.
                    pshs      u
                    ldx       #FN$ParseWait
                    os9       F$Sleep
                    puls      u
                    clrb
                    andcc     #^Carry
                    rts
FNParseExit         rts

* Query the parsed JSON document.
* Entry: X=query string, Y=destination, B=destination capacity incl. NUL.
* Exit: destination is always NUL terminated on success.
FNNetQuery          sty       fndest,u
                    stb       fncapacity,u
                    lbeq      FNBadArg
                    pshs      x
                    lbsr      FNClearReq
                    leax      fnreq,u
                    lda       #OP_FUJINET
                    sta       2,x
                    lda       #FN$Unit
                    sta       3,x
                    lda       #FN$Query
                    sta       4,x
                    puls      x
                    leay      fnreq+7,u
                    lbsr      FNCopy255
                    lbcs      FNQueryExit
                    leax      fnreq,u
                    ldy       #FN$ReqSize
* The reference FujiNet clients send Q and immediately request a bounded value
* buffer.  Do not insert a Status exchange here: on the RS-232 path its
* four-byte response can arrive late and shift the following value stream.
                    lbsr      FNSend
                    lbcs      FNQueryExit

* Read one caller-sized, NUL-padded value buffer.  Apart from avoiding an
* unnecessary 256-byte transfer for scalar values, this keeps Level 2's
* system/user F$Move away from the problematic exact-$0100 boundary.
                    lbsr      FNBuildShort
                    lda       #FN$Read
                    sta       fnreq+4,u
                    clra
                    ldb       fncapacity,u
                    std       fnreadlen,u
                    std       fnreq+5,u
                    leax      fnreq,u
                    ldy       #7
                    lbsr      FNSend
                    bcs       FNQueryExit
                    ldd       fnreadlen,u
                    leay      fnresp,u
                    lbsr      FNGetResponse
                    bcs       FNQueryExit

* Copy at most capacity-1 bytes, stopping at NUL, CR, or LF.
                    ldb       fncapacity,u
                    decb
                    stb       fncopy,u
                    leax      fnresp,u
                    ldy       fndest,u
FNCopyValue         tst       fncopy,u
                    beq       FNCopyValueDone
                    ldd       fnavail,u
                    beq       FNCopyValueDone
                    lda       ,x+
                    beq       FNCopyValueDone
                    cmpa      #C$CR
                    beq       FNCopyValueDone
                    cmpa      #C$LF
                    beq       FNCopyValueDone
                    sta       ,y+
                    dec       fncopy,u
                    ldd       fnavail,u
                    subd      #1
                    std       fnavail,u
                    bra       FNCopyValue
FNCopyValueDone     clr       ,y
FNQueryOK           clrb
                    andcc     #^Carry
FNQueryExit         rts

FNTooLarge          ldb       #E$BufSiz
                    orcc      #Carry
                    rts
FNBadArg            ldb       #E$IllArg
                    orcc      #Carry
                    rts

********************************************************************
* Transaction helpers

FNBuildShort        leax      fnreq,u
                    ldy       #7
FNZeroShort         clr       ,x+
                    leay      -1,y
                    bne       FNZeroShort
                    lda       #OP_FUJINET
                    sta       fnreq+2,u
                    lda       #FN$Unit
                    sta       fnreq+3,u
                    rts

FNClearReq          leax      fnreq,u
                    ldy       #FN$ReqSize
FNClearLoop         clr       ,x+
                    leay      -1,y
                    bne       FNClearLoop
                    rts

* Entry X=source, Y=destination. Copies at most 255 bytes plus NUL.
FNCopy255           ldb       #255
FNCopyLoop          lda       ,x+
                    beq       FNCopyDone
                    sta       ,y+
                    decb
                    bne       FNCopyLoop
                    tst       ,x
                    bne       FNTooLarge
FNCopyDone          clr       ,y
                    clrb
                    andcc     #^Carry
                    rts

* Send a command and then request its one-byte network status.
FNSendCheck         lbsr      FNSend
                    bcs       FNSendExit
                    lbra      FNGetError
FNSendExit          rts

* Entry X=prefix+request block, Y=total block length.
*
* Do not precede OP_FUJINET commands with OP_FUJI/Ready.  On the RS-232
* firmware the Ready byte is returned asynchronously; it can arrive after
* the following E3 request and be mistaken for the first byte of its result.
FNSend
                    pshs      x                   retain the request block
                    leax      fnresp,u
                    stx       fnout,u
                    puls      x                   FNXfer consumes the request in X
                    lbsr      FNXfer
                    bcc       FNSendDone
* Some SS.Fuji implementations have reported Carry set with B=$00 after
* a no-response command has already reached FujiNet.  Zero is not an OS-9
* error code, so preserve real errors but normalize that impossible result.
                    tstb
                    bne       FNSendDone
                    andcc     #^Carry
FNSendDone           rts

FNGetError          lbsr      FNBuildShort
                    ldd       #1
                    std       fnreq,u
* Network result commands stay entirely on OP_FUJINET.  A global OP_FUJI
* Ready reply is asynchronous on the RS-232 transport and would shift this
* raw E3 reply by one byte.
                    lda       #FN$Error
                    sta       fnreq+4,u
                    leax      fnerror,u
                    stx       fnout,u
                    leax      fnreq,u
                    ldy       #7
                    lbsr      FNXfer
                    bcs       FNErrorExit
                    ldb       fnerror,u
                    cmpb      #FN$Success
                    beq       FNErrorOK
                    orcc      #Carry
FNErrorExit         rts
FNErrorOK           clrb
                    andcc     #^Carry
                    rts

* Entry D=response size, Y=response destination.
FNGetResponse       std       fnavail,u
                    sty       fnout,u
                    lbsr      FNBuildShort
                    ldd       fnavail,u
                    std       fnreq,u
* Keep network results in the OP_FUJINET stream.  A global OP_FUJI/$01
* response request consumes the preceding network result byte first on the
* RS-232 firmware path and shifts the value by one byte.
                    lda       #FN$Response
                    sta       fnreq+4,u
                    ldd       fnavail,u
                    std       fnreq+5,u
                    leax      fnreq,u
                    ldy       #7
                    lbra      FNXfer

* scdwv SS.Fuji uses R$U as the response pointer; preserve the data base.
FNXfer              pshs      u
                    sty       fnxferlen,u
                    lda       fnpath,u
                    ldb       #SS.Fuji
                    ldu       fnout,u
                    os9       I$SetStt
                    puls      u
                    bcs       FNXferError
                    clr       fnxfererr,u
                    rts
FNXferError         stb       fnxfererr,u
                    rts

                    endsect
