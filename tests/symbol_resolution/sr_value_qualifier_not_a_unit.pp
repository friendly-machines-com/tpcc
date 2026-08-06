program sr_value_qualifier_not_a_unit;

{ TARGET rule 3 case 4: when the LHS of `.` is neither a unit, a record/
  object/class value, nor a usable type identifier, the dotted form is an
  error. Here `I` is an Integer, which has no members. }

var
  I: Integer;

begin
  I := 0;
  I.Field := 0
end.
