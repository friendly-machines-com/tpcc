program sr_qualified_unit_no_such_member;

{ TARGET rule 3 sanity: once the LHS resolves to a unit, the RHS is looked up
  in that unit's interface frame; if there is no symbol of that name there,
  the diagnostic names the unit and the missing member. }

uses sr_kitchen_sink;

var
  X: Integer;

begin
  X := sr_kitchen_sink.NoSuchMember
end.
