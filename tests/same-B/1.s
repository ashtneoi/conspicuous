            movlb 0
            movf 0x0C, W
            movlb 1
            iorwf 0x8C, W
            movlb 0
            movwf 0x0C
