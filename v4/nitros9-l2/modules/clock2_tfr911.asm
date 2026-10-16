********************************************************************
* Clock2 - TFR/911H real-time clock driver
*
* The TFR/911H firmware presents the PC's wall-clock time (sent by the
* tether) at $FFC0-$FFC7 in the format of Jeff Vavasour's CoCo 3 emulator
* RTC: century, year, month, day, weekday, hour, minute, second (binary).
*
* Based on clock2_jvemu.asm, but Init and SetTime also read the RTC, so the
* time is right from boot, and setime cannot drift it from the PC's clock.

                    nam       Clock2
                    ttl       TFR/911H RTC Driver

                    ifp1
                    use       defsfile
                    endc

tylg                set       Sbrtn+Objct
atrv                set       ReEnt+rev
rev                 set       $00
edition             set       1

RTC.Base            equ       $FFC0

                    mod       eom,name,tylg,atrv,JmpTable,RTC.Base

name                fcs       "Clock2"
                    fcb       edition

JmpTable            bra       GetTime             Init
                    nop
                    bra       GetTime             GetTime
                    nop
                    bra       GetTime             SetTime: the PC's clock wins

GetTime             ldx       #RTC.Base
                    ldd       ,x                  get year (CCYY)
                    suba      #20
                    bmi       yr1                 19xx, OK as is
yr0                 addb      #100                20xx adjustment
                    deca                          also check for
                    bpl       yr0                 21xx (optional)
yr1                 stb       <D.Year             set year (~YY)
                    ldd       2,x                 get date
                    std       <D.Month            set date (MMDD)
                    ifne      Level-1
                    ldd       4,x                 get time (wwhh)
                    sta       <D.Daywk            set day of week
                    else
                    ldb       5,x                 get hour (hh)
                    endc
                    stb       <D.Hour             set hour (hh)
                    ldd       6,x                 get time (mmss)
                    std       <D.Min              set time (mmss)
                    rts

                    emod
eom                 equ       *
                    end
