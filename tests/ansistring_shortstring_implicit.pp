program AnsiStringShortStringImplicit;

type
  TTiny = string[3];

var
  Captured: TTiny;
  Value: AnsiString;

procedure TakeTiny(const Value: TTiny);
begin
  Captured := Value
end;

begin
  Value := 'abcd';
  TakeTiny(Value);
  if Captured <> 'abc' then
    Halt(1)
end.
