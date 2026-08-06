program sr_type_in_value_position;

{ TARGET rule 6b: a type identifier has no value semantics. Using one as the
  RHS of an assignment is rejected. }

type
  TInt = Integer;

var
  X: Integer;

begin
  X := TInt
end.
