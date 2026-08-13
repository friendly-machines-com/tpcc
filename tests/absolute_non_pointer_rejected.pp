program AbsoluteNonPointerRejected;
procedure TakeInt(arg: LongInt);
var
  ip: LongInt absolute arg;
begin
  ip := 0
end.
