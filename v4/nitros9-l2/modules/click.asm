********************************************************************
* click - turn keyclicks on or off: click on | click off
*
* Sets bit 0 of the TFR/911H sound control register at $FF42.

                    nam       click
                    ttl       Keyclicks on or off

                    ifp1
                    use       defsfile
                    endc

tylg                set       Prgrm+Objct
atrv                set       ReEnt+rev
rev                 set       $00

                    mod       eom,name,tylg,atrv,start,size

SndCtl              equ       $FF42

                    org       0
                    rmb       256                 stack
size                equ       .

name                fcs       /click/
                    fcb       1                   edition

usage               fcc       "Usage: click on|off"
                    fcb       C$CR
usagelen            equ       *-usage

start               lda       ,x+                 skip spaces
                    cmpa      #C$SPAC
                    beq       start
                    anda      #$DF                upper case
                    cmpa      #'O
                    bne       showuse
                    lda       ,x                  "oN" or "oFf"?
                    anda      #$DF
                    cmpa      #'N
                    beq       on
                    cmpa      #'F
                    bne       showuse
                    lda       >SndCtl
                    anda      #$FE
                    bra       set
on                  lda       >SndCtl
                    ora       #$01
set                 sta       >SndCtl
                    clrb
                    bra       exit
showuse             leax      usage,pcr
                    ldy       #usagelen
                    lda       #2                  standard error
                    os9       I$WritLn
                    clrb
exit                os9       F$Exit

                    emod
eom                 equ       *
                    end
