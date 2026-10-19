********************************************************************
* fnansi - ANSI terminal output for fnconfig and fnweather
*
* The apps drive a CoCo 3 window (CoWin) with its control codes:
* $02 x y to move the cursor, $0C to clear, $03 to clear the line,
* $05 for the cursor and $1F for reverse video.  Assembled with
* ANSI=1, they use these routines instead, which send the ANSI
* (VT100) sequences a terminal emulator understands.  The TFR/911H's
* console is the terminal its tether runs in.
*
* The including file supplies WriteOut (X = bytes, Y = count, to
* stdout) and posbuf (at least 9 bytes).
*
* @license gpl v. 3, see fujinet/LICENSE for details.
*
* Edt/Rev  YYYY/MM/DD  Modified by
* Comment
* ------------------------------------------------------------------
*   1      2026/10/17  Andrew Diller
* Created for the TFR/911H.

TClear              leax      ansiClear,pcr
                    bra       AnsiOut
ClearLine           leax      ansiClearLine,pcr
                    bra       AnsiOut
CursorOff           leax      ansiCursorOff,pcr
                    bra       AnsiOut
CursorOn            leax      ansiCursorOn,pcr
                    bra       AnsiOut
ReverseOn           leax      ansiReverseOn,pcr
                    bra       AnsiOut
ReverseOff          leax      ansiReverseOff,pcr

* Write the NUL-terminated sequence at X.
AnsiOut             pshs      x
                    ldy       #0
count@              tst       ,x+
                    beq       write@
                    leay      1,y
                    bra       count@
write@              puls      x
                    lbra      WriteOut

* TAt - move the cursor to column A, row B (both from 0):  ESC [ row ; col H
TAt                 pshs      d
                    leax      posbuf,u
                    ldd       #$1B5B              ESC [
                    std       ,x++
                    ldb       1,s                 row
                    incb                          ANSI counts from 1
                    bsr       AnsiDec
                    lda       #';
                    sta       ,x+
                    ldb       ,s                  column
                    incb
                    bsr       AnsiDec
                    ldd       #'H*256             H, NUL
                    std       ,x
                    leas      2,s
                    leax      posbuf,u
                    bra       AnsiOut

* Store B (0-99) at X as one or two decimal digits.
AnsiDec             lda       #'0-1
tens@               inca
                    subb      #10
                    bcc       tens@
                    addb      #10+'0
                    cmpa      #'0
                    beq       ones@
                    sta       ,x+
ones@               stb       ,x+
                    rts

ansiClear           fcb       $1B,'[,'H,$1B,'[,'2,'J,0
ansiClearLine       fcb       $1B,'[,'K,0
ansiCursorOff       fcb       $1B,'[,'?,'2,'5,'l,0
ansiCursorOn        fcb       $1B,'[,'?,'2,'5,'h,0
ansiReverseOn       fcb       $1B,'[,'7,'m,0
ansiReverseOff      fcb       $1B,'[,'0,'m,0
