program function_pointer_direct_method_equality_rejected;

type
  TProcedure = procedure of object;

var
  A: TProcedure;
  B: TProcedure;
  Equal: Boolean;

begin
  Equal := A = B
end.
