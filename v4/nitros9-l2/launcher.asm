; launcher.asm -- 256 bytes at [$2500,$2600), entered from the RESET vector.
; Disable interrupts, start a stack, and jump to rel's entry at $2602,
; the same way Disk BASIC's DOS command starts a track-35 kernel image.
; (From v3.decprecated/launchers/launch-2500-to-2602.asm.)

    ORG $2500

    orcc #$50           ; disable interrupts
    lds #$0500          ; initial system stack
    jmp $2602           ; entry to rel

    fill '?,$2600-*     ; pad to 256 bytes

    END $2500
