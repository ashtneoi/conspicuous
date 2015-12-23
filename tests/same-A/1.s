            .sfr 0x00C, A
            .sfr 0x08C, B

            movf A, W
            iorwf B, W
            movwf A
