program PCharShortStringCheckedOverflow;

{$R+}

type
  TTiny = string[3];

var
  Buffer: array[0..4] of Char;
  PointerValue: PChar;
  Value: TTiny;

procedure TakeTiny(const Argument: TTiny);
begin
  Value := Argument
end;

begin
  Buffer[0] := 'a';
  Buffer[1] := 'b';
  Buffer[2] := 'c';
  Buffer[3] := 'd';
  Buffer[4] := #0;
  PointerValue := @Buffer[0];

{$ifdef CHECK_CALL}
  TakeTiny(PointerValue)
{$else}
  Value := PointerValue
{$endif}
end.
