program AbsoluteSizeMismatchRejected;
procedure TakePtr(arg: Pointer);
var
  b: Byte absolute arg;
begin
  b := 0
end.
