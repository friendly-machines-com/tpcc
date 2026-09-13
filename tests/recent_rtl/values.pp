program RecentRTLValues;
uses SysUtils;
var
  W: LongWord;
  P: PLongWord;
  C: PCardinal;
  S: AnsiString;
  Short: ShortString;
  I: Integer;
  Mask: TFPUExceptionMask;
begin
  W := 42; P := @W; C := @W;
  if (P^ <> 42) or (C^ <> 42) then Halt(1);
  if vtAnsiString <> 11 then Halt(2);
  if (ExtensionSeparator <> '.') or (fmShareDenyNone <> $40) then Halt(3);
  Mask := [exInvalidOp, exPrecision];
  if not (exInvalidOp in Mask) or (exZeroDivide in Mask) then Halt(4);
  if (Ord(exInvalidOp) <> 0) or (Ord(exDenormalized) <> 1) or
     (Ord(exZeroDivide) <> 2) or (Ord(exOverflow) <> 3) or
     (Ord(exUnderflow) <> 4) or (Ord(exPrecision) <> 5) then Halt(5);
  if UpCase('z') <> 'Z' then Halt(6);
  Short := 'aZ09!';
  if UpCase(Short) <> 'AZ09!' then Halt(7);
  if not Odd(ShortInt(-3)) or Odd(Byte(2)) or not Odd(SmallInt(-1)) or
     Odd(Word(65534)) or not Odd(LongInt(-3)) or Odd(LongWord(42)) or
     not Odd(Int64(-1)) or not Odd(High(QWord)) then Halt(8);
  S := 'abc'#0'ignored';
  if System.StrPas(PChar(S)) <> 'abc' then Halt(9);
  S := StringOfChar('x', 300);
  if Length(S) <> 300 then Halt(10);
  for I := 1 to 300 do if S[I] <> 'x' then Halt(11);
  if Length(System.StrPas(PChar(S))) <> 255 then Halt(12);
  if StringOfChar('x', 0) <> '' then Halt(13);
  S := StringOfChar(#0, 3);
  if (Length(S) <> 3) or (S[2] <> #0) then Halt(14);
  WriteLn('values ok')
end.
