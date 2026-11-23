REM RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
REM Counts how many more files can be opened (FileSwitch handles), then closes them again
DIM h%(300)
n%=0
ON ERROR PRINT "stopped after ";n%;": ";REPORT$: PROCclose: END
REPEAT
  h%(n%)=OPENIN("SDFS::RISCOSpi.$.code.freehandles")
  IF h%(n%)=0 THEN PRINT "OPENIN gave 0 after ";n%: PROCclose: END
  n%+=1
UNTIL n%=300
PRINT "opened 300": PROCclose: END
DEF PROCclose: LOCAL i%: FOR i%=0 TO n%-1: CLOSE#h%(i%): NEXT: ENDPROC
