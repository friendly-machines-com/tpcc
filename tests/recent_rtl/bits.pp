program RecentRTLBits;
var
  B: Byte;
  W: Word;
  D: DWord;
  Q: QWord;
  I: Integer;
begin
  for I := 0 to 255 do
    begin
      B := I;
      if RolByte(B) <> RolByte(B, 1) then Halt(1);
      if RorByte(B) <> RorByte(B, 1) then Halt(2);
      if RorByte(RolByte(B, 3), 3) <> B then Halt(3);
      if RolByte(B, 8) <> B then Halt(4)
    end;
  W := $8001; D := $80000001; Q := $8000000000000001;
  if (RolWord(W) <> 3) or (RorWord(W) <> $c000) then Halt(5);
  if (RolDWord(D) <> 3) or (RorDWord(D) <> $c0000000) then Halt(6);
  if (RolQWord(Q) <> 3) or (RorQWord(Q) <> $c000000000000000) then Halt(7);
  for I := 0 to 255 do
    begin
      if RorWord(RolWord(W, I), I) <> W then Halt(8);
      if RorDWord(RolDWord(D, I), I) <> D then Halt(9);
      if RorQWord(RolQWord(Q, I), I) <> Q then Halt(10)
    end;
  for I := 0 to 63 do
    begin
      Q := QWord(1) shl I;
      if (BsfQWord(Q) <> I) or (BsrQWord(Q) <> I) or (PopCnt(Q) <> 1) then Halt(11);
      if I < 32 then
        begin
          D := DWord(Q);
          if (BsfDWord(D) <> I) or (BsrDWord(D) <> I) then Halt(12)
        end;
      if I < 16 then
        begin
          W := Word(Q);
          if (BsfWord(W) <> I) or (BsrWord(W) <> I) then Halt(13)
        end;
      if I < 8 then
        begin
          B := Byte(Q);
          if (BsfByte(B) <> I) or (BsrByte(B) <> I) then Halt(14)
        end
    end;
  if (PopCnt(Byte(255)) <> 8) or (PopCnt(Word(65535)) <> 16) or
     (PopCnt(High(DWord)) <> 32) or (PopCnt(High(QWord)) <> 64) then Halt(15);
  if PopCnt(QWord(0)) <> 0 then Halt(16);
  WriteLn('bits ok')
end.
