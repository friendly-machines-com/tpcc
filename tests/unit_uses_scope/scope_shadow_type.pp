program Scope_Shadow_Type;
uses Scope_B;
type
  Scope_B = Integer;
var
  X: Scope_B;
begin
  X := 7;
  if X <> 7 then
    Halt(1)
end.
