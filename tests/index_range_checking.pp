program IndexRangeChecking;

uses SysUtils;

var
  Values: array[1..2] of Integer;
  Character: Char;
  OrdinalByte: Byte;
  Caught: Boolean;

begin
  {$R-}
  Character := Chr(42);
  OrdinalByte := Character;
  if OrdinalByte <> 42 then
    Halt(3);
  Values[1] := 40;
  {$R+}
  OrdinalByte := Character;
  if OrdinalByte <> 42 then
    Halt(4);
  Values[2] := 2;
  if Values[1] + Values[2] <> 42 then
    Halt(1);

  Caught := False;
  try
    Values[3] := 0
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(2)
end.
