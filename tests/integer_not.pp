program IntegerNot;

const
  InferredMask = not 4095;
  ByteMask: Byte = not Byte(15);

var
  B: Byte;
  SI: ShortInt;
  W: Word;
  S: SmallInt;
  C: Cardinal;
  I: Integer;
  Q: QWord;
  I64: Int64;
  N: Integer;

procedure Fail;
var
  Zero: Integer;
begin
  Zero := 0;
  N := 1 div Zero
end;

begin
  if InferredMask <> -4096 then
    Fail;
  if ByteMask <> 240 then
    Fail;

  B := 15;
  B := not B;
  if B <> 240 then
    Fail;

  SI := 15;
  SI := not SI;
  if SI <> -16 then
    Fail;

  W := 15;
  W := not W;
  if W <> 65520 then
    Fail;

  S := 15;
  S := not S;
  if S <> -16 then
    Fail;

  C := 4095;
  C := not C;
  if C <> $fffff000 then
    Fail;

  I := 4095;
  I := not I;
  if I <> -4096 then
    Fail;

  Q := 4095;
  Q := not Q;
  if Q <> $fffffffffffff000 then
    Fail;

  I64 := 4095;
  I64 := not I64;
  if I64 <> -4096 then
    Fail;

  I := 4097;
  I := (I + (4096 - 1)) and not (4096 - 1);
  if I <> 8192 then
    Fail
end.
