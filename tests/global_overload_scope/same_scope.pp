program GlobalOverloadSameScope;

function Pick(Value: Integer): Integer;
begin
  Pick := Value + 9
end;

function Pick(Value: ShortString): Integer;
begin
  Pick := Length(Value) + 19
end;

begin
  if Pick(1) <> 10 then
    Halt(1);
  if Pick('x') <> 20 then
    Halt(2)
end.
