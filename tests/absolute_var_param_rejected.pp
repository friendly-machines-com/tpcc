program AbsoluteVarParamRejected;
type
  PInt = ^LongInt;
procedure TakePtr(var arg: Pointer);
var
  ip: PInt absolute arg;
begin
  ip^ := 0
end.
