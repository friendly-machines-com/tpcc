program sr_qualified_kinds;

{ TARGET rule 3: every kind of symbol is reachable through `Unit.Symbol`:
  type, const, var, procedure, function, enum literal. Chained member access
  on a value of a unit-qualified record type exercises `Unit.Type` followed
  by `.Field` (rule 3f). }

uses sr_kitchen_sink;

var
  R: sr_kitchen_sink.TRec;        { Unit.Type }
  S: sr_kitchen_sink.TShade;      { Unit.Type, different type }
  I: Integer;

begin
  I := sr_kitchen_sink.CAnswer;   { Unit.Const }
  sr_kitchen_sink.GCounter := I;  { Unit.Var }
  sr_kitchen_sink.Bump(I);        { Unit.Proc }
  R := sr_kitchen_sink.Make(7);   { Unit.Func returning TRec }
  S := sr_kitchen_sink.ShGreen;   { Unit.EnumLit }

  if R.F <> 7 then Halt(1);       { chained Unit.Type.Member via value }
  if I <> 43 then Halt(2);
  if S <> sr_kitchen_sink.ShGreen then Halt(3);
  if sr_kitchen_sink.GCounter <> 42 then Halt(4)
end.
