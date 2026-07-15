program Scope_Explicit;
uses Scope_X, Scope_B;
type
  TFill = procedure(var X: Scope_B.TBInteger);
var
  X: Scope_B.TBInteger;
  Shade: Scope_B.TShade;
  Fill: TFill;
begin
  X := Scope_B.BValue;
  Shade := Scope_B.ShadeGreen;
  Fill := @Scope_X.FillFromDependencies;
  Fill(X);
  if X <> 112 then
    Halt(1)
end.
