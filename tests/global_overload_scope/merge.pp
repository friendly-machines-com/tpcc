program GlobalOverloadMerge;

uses OverloadScopeUnit;

function Pick(Value: ShortString): Integer; overload;
begin
  Pick := Length(Value) + 19
end;

begin
  if Pick(1) <> 10 then
    Halt(1);
  if Pick('x') <> 20 then
    Halt(2)
end.
