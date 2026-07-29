program function_pointer_explicit_nonpointer_rejected;

type
  TIntegerCallback = procedure(Value: Integer);
  TRealCallback = procedure(Value: Double);

var
  IntegerCallback: TIntegerCallback;
  RealCallback: TRealCallback;

begin
  IntegerCallback := TIntegerCallback(RealCallback)
end.
