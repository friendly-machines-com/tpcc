program sr_unit_name_shadowable;

{ TARGET rule 5: a unit name is an ordinary identifier. A local declaration
  of the same name shadows the unit for unqualified lookup and turns the
  dotted form into member access on the local. }

uses sr_kitchen_sink;

type
  TLocal = record
    F: Integer
  end;

var
  sr_kitchen_sink: TLocal;   { shadows the unit name locally }

begin
  sr_kitchen_sink.F := 9;    { member access on the var, not Unit.Type }
  if sr_kitchen_sink.F <> 9 then Halt(1)
end.
