program function_pointer_implicit_pointer_mismatch_rejected;

type
  TObjectCallback = procedure(Data: TObject);
  TPointerCallback = procedure(Data: Pointer);

var
  ObjectCallback: TObjectCallback;
  PointerCallback: TPointerCallback;

begin
  PointerCallback := ObjectCallback
end.
