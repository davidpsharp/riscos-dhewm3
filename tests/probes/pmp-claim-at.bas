REM Claims pages at different PMP page indexes (as V3DRender's BO area does), to see where claims fail
pages%=65536
SYS "XOS_DynamicArea",0,-1,0,-1,(1<<7) OR (1<<8) OR (1<<20),pages%*4096,0,-1,"PMP probe",pages% TO e%,da%,,base%;f%
IF f% AND 1 THEN PRINT "create failed: ";$(e%+4): END
PRINT "PMP "+STR$da%+" at &";~base%;", ";pages%;" pages"
DIM l% 16*12
FOR t%=1 TO 8
  READ at%,n%
  FOR i%=0 TO n%-1: l%!(i%*12)=at%+i%: l%!(i%*12+4)=-2: l%!(i%*12+8)=1<<15: NEXT
  SYS "XOS_DynamicArea",21,da%,l%,n% TO r0%;f%
  IF f% AND 1 THEN PRINT "claim ";n%;" at ";at%;": ERROR ";$(r0%+4) ELSE PRINT "claim ";n%;" at ";at%;": ok"
  IF (f% AND 1)=0 THEN FOR i%=0 TO n%-1: l%!(i%*12)=at%+i%: l%!(i%*12+4)=-1: l%!(i%*12+8)=0: NEXT: SYS "XOS_DynamicArea",21,da%,l%,n%
NEXT
SYS "XOS_DynamicArea",1,da%
END
DATA 0,1, 0,16, 1000,16, 16384,1, 32767,1, 32768,1, 65520,16, 65535,1
