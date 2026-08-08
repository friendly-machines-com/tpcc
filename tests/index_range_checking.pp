program IndexRangeChecking;

uses SysUtils;

var
  Values: array[1..2] of Integer;
  Character: Char;
  WiderOrdinal: Word;
  OrdinalByte: Byte;
  Caught: Boolean;

begin
  {$R-}
  WiderOrdinal := 42;
  OrdinalByte := WiderOrdinal;
  if OrdinalByte <> 42 then
    Halt(5);
  Values[1] := 40;
  {$R+}
  // Word has a wider domain than Byte. Even though this particular runtime
  // value fits, {$R+} must check before committing it to the destination.
  OrdinalByte := WiderOrdinal;
  if OrdinalByte <> 42 then
    Halt(6);
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
