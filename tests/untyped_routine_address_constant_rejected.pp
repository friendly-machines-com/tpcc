program UntypedRoutineAddressConstantRejected;

procedure Callback;
begin
end;

const
  CallbackAddress = @Callback;

begin
end.
