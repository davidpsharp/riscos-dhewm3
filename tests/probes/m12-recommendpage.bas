REM RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
REM OS_Memory 12 with bit 9 (1 GB to 4 GB), then OS_Memory 0: page number to physical address
DIM b% 12
size%=4096
REPEAT
  SYS "XOS_Memory",12 OR (1<<9),size%,12,0,&40000000,0,&FFFFFFFF,0 TO ,,,page%;f%
  IF f% AND 1 THEN PRINT "size ";size%;": refused" ELSE b%!0=page%:b%!4=0:b%!8=0:SYS "XOS_Memory",(1<<8) OR (1<<13),b%,1 TO ;g%:IF g% AND 1 THEN PRINT "size ";size%;": page ";page%;" (no physical address)" ELSE PRINT "size ";size%;": page ";page%;" physical &";~b%!8
  size%=size%*4
UNTIL size%>4194304
SYS "XOS_Memory",12,65536,12 TO ,,,page%
b%!0=page%:SYS "XOS_Memory",(1<<8) OR (1<<13),b%,1
PRINT "no range, 64 KB: page ";page%;" physical &";~b%!8
