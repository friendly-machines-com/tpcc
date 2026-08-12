program StrPasTest;

uses
  SysUtils;

var
  Buffer: array[0..300] of Char;
  I: Integer;
  Value: AnsiString;

begin
  for I := 0 to 299 do
    Buffer[I] := Chr(Ord('a') + I mod 26);
  Buffer[300] := #0;

  Value := SysUtils.StrPas(@Buffer[0]);
  if Length(Value) <> 300 then
    Halt(1);
  if (Value[1] <> 'a') or
     (Value[26] <> 'z') or
     (Value[300] <> Chr(Ord('a') + 299 mod 26)) then
    Halt(2);

  Buffer[3] := #0;
  Value := SysUtils.StrPas(@Buffer[0]);
  if Value <> 'abc' then
    Halt(3);

  Value := SysUtils.StrPas(nil);
  if Value <> '' then
    Halt(4)
end.
