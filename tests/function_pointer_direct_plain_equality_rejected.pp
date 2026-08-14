program function_pointer_direct_plain_equality_rejected;

type
  TProcedure = procedure;

var
  A: TProcedure;
  B: TProcedure;
  Equal: Boolean;

begin
  Equal := A = B
end.
