program GlobalOverloadShadow;

uses OverloadScopeUnit;

function Pick(Value: ShortString): Integer;
begin
  Pick := Length(Value) + 19
end;

begin
  if Pick('x') <> 20 then
    Halt(1);
  Pick(1)
end.
