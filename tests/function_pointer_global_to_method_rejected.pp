program function_pointer_global_to_method_rejected;

type
  TBound = procedure(Value: Integer) of object;

var
  Bound: TBound;

procedure Global(Value: Integer);
begin
end;

begin
  Bound := @Global
end.
