program function_pointer_explicit_cross_kind_rejected;

type
  TPlain = procedure(Value: Integer);
  TBound = procedure(Value: Integer) of object;

var
  Plain: TPlain;
  Bound: TBound;

begin
  Plain := TPlain(Bound)
end.
