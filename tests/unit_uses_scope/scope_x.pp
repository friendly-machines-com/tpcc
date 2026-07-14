unit Scope_X;
interface
uses Scope_B;
type
  TXInteger = TBInteger;
const
  ShadowedValue = 2;
procedure FillFromDependencies(var X: TXInteger);
implementation
uses Scope_C;
procedure FillFromDependencies;
begin
  X := ShadowedValue + BValue + CValue
end;
end.
