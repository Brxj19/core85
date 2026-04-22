ORG 2000H

START: LXI H,DATA
       MOV A,M
       INX H
       MOV B,M
       SHLD PTR
       LDA PTR
       HLT

PTR:   DW 0000H
DATA:  DB 12H,34H
