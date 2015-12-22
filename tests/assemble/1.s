            .sfr 0x00C, A
            .sfr 0x00D, B
            .sfr 0x00E, C
            .sfr 0x08C, X

            movlb X
            movf A, W
            andlw 0xF0
            movwf B
            movf C, w
            andlw 0x0F
            iorwf B
