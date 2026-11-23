REM RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
REM Prints the free pool and every dynamic area of 4 MB or more (size of max, MB)
SYS "OS_ReadDynamicArea",6 TO ,fp%
PRINT "free pool ";fp% DIV 1048576;" MB"
n%=-1
REPEAT
  SYS "XOS_DynamicArea",3,n% TO ,n%;f%
  IF (f% AND 1)=0 AND n%<>-1 THEN PROCshow(n%)
UNTIL n%=-1 OR (f% AND 1)
END
DEF PROCshow(n%)
LOCAL size%,max%,name%,f%,a$,i%
SYS "XOS_DynamicArea",2,n% TO ,,size%,,,max%,,,name%;f%
IF f% AND 1 THEN ENDPROC
IF size%<4194304 AND max%<268435456 THEN ENDPROC
a$="":i%=0
WHILE name%?i%>=32 AND i%<40: a$+=CHR$(name%?i%): i%+=1: ENDWHILE
PRINT "  ";n%;" ";a$;": ";size% DIV 1048576;" of ";max% DIV 1048576
ENDPROC
