********************************************************************
* beep - ring the terminal bell (on the TFR/911H, also its speaker)

                    nam       beep
                    ttl       Ring the bell

                    ifp1
                    use       defsfile
                    endc

tylg                set       Prgrm+Objct
atrv                set       ReEnt+rev
rev                 set       $00

                    mod       eom,name,tylg,atrv,start,size

                    org       0
                    rmb       256                 stack
size                equ       .

name                fcs       /beep/
                    fcb       1                   edition

bell                fcb       C$BELL

start               leax      bell,pcr
                    ldy       #1
                    lda       #1                  standard output
                    os9       I$Write
                    bcs       exit
                    clrb
exit                os9       F$Exit

                    emod
eom                 equ       *
                    end
