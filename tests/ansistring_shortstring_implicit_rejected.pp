program AnsiStringShortStringImplicitRejected;

type
  TTiny = string[3];

procedure TakeTiny(const Value: TTiny);
begin
end;

var
  Value: AnsiString;

begin
  TakeTiny(Value)
end.
