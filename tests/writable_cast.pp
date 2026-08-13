program WritableCast;

type
  TDoubleBytes = array[3..10] of Byte;

var
  C: Char;
  Signed: ShortInt;
  Unsigned: Byte;
  Values: array[1..2] of ShortInt;
  IndexCalls: Integer;
  RealValue: Double;
  RealBytes: TDoubleBytes;
  OriginalFirst: Byte;
  ChangedFirst: Byte;
  ChangedByVar: Byte;

procedure FlipByte(var Value: Byte);
begin
  Value := Value xor $ff
end;

function NextIndex: Integer;
begin
  Inc(IndexCalls);
  Result := 1
end;

begin
  C := #0;
  Byte(C) := 255;

  Signed := 0;
  Byte(Signed) := 255;

  Unsigned := 0;
  ShortInt(Unsigned) := -1;

  Values[1] := 0;
  Byte(Values[NextIndex]) := 255;

  RealValue := 1.5;
  RealBytes := TDoubleBytes(RealValue);
  OriginalFirst := TDoubleBytes(RealValue)[3];
  TDoubleBytes(RealValue)[3] := OriginalFirst xor $ff;
  ChangedFirst := TDoubleBytes(RealValue)[3];
  TDoubleBytes(RealValue) := RealBytes;
  FlipByte(TDoubleBytes(RealValue)[4]);
  ChangedByVar := TDoubleBytes(RealValue)[4];
  TDoubleBytes(RealValue) := RealBytes
end.
