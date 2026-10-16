********************************************************************
* clear - clear the terminal screen (ANSI: cursor home, erase screen)

                    nam       clear
                    ttl       Clear the screen

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

name                fcs       /clear/
                    fcb       1                   edition

seq                 fcb       $1B
                    fcc       "[H"
                    fcb       $1B
                    fcc       "[2J"
seqlen              equ       *-seq

start               leax      seq,pcr
                    ldy       #seqlen
                    lda       #1                  standard output
                    os9       I$Write
                    bcs       exit
                    clrb
exit                os9       F$Exit

                    emod
eom                 equ       *
                    end
