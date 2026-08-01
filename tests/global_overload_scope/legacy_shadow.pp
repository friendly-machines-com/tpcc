program LegacyBuiltinShadow;

var
  Seen: Integer;

procedure Write(Value: ShortString);
begin
  Seen := Length(Value)
end;

begin
  Write('hello');
  if Seen <> 5 then
    Halt(1)
end.
