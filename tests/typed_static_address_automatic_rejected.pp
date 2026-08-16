program TypedStaticAddressAutomaticRejected;

procedure RejectAutomatic;
var
  LocalValue: LongInt;
const
  LocalAddress: ^LongInt = @LocalValue;
begin
  LocalAddress^ := 1
end;

begin
  RejectAutomatic
end.
