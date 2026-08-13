program AbsoluteAlias;
type
  PInt = ^LongInt;
procedure TakePtr(arg: Pointer);
var
  ip: PInt absolute arg;
begin
  ip^ := ip^ * 2
end;
var
  x: LongInt;
begin
  x := 21;
  TakePtr(@x);
  if x <> 42 then Halt(1)
end.
