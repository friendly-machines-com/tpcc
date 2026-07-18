program BooleanShortCircuit;

var
  Calls: Integer;
  AndValue: Boolean;
  OrValue: Boolean;

function Observe(Value: Boolean): Boolean;
begin
  Inc(Calls);
  Result := Value
end;

begin
  Calls := 0;
  AndValue := False and Observe(True);
  OrValue := True or Observe(False);
  if AndValue or
     not OrValue or
     (Calls <> 0) then
    Halt(1);

  AndValue := True and Observe(True);
  if not AndValue or
     (Calls <> 1) then
    Halt(2);

  OrValue := False or Observe(True);
  if not OrValue or
     (Calls <> 2) then
    Halt(3)
end.
