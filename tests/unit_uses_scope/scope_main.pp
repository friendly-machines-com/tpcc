program Scope_Main;
uses Scope_X;
var
  ShadowedValue: Integer;
  X: TXInteger;
begin
  ShadowedValue := 7;
  X := 0;
  FillFromDependencies(X);
  if X <> 112 then
    Halt(1);
  if ShadowedValue <> 7 then
    Halt(2)
end.
