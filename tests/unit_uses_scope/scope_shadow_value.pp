program Scope_Shadow_Value;
uses Scope_B;
type
  TLocal = record
    BValue: Integer
  end;
var
  Scope_B: TLocal;
begin
  Scope_B.BValue := 9;
  if Scope_B.BValue <> 9 then
    Halt(1)
end.
