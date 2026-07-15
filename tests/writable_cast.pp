program WritableCast;

var
  C: Char;
  Signed: ShortInt;
  Unsigned: Byte;
  Values: array[1..2] of ShortInt;
  IndexCalls: Integer;

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
  Byte(Values[NextIndex]) := 255
end.
