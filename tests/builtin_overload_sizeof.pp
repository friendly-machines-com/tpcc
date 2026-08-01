program BuiltinOverloadSizeOf;

{ Same-scope global routines overload automatically. This nearer declaration
  does not implicitly open the outer legacy System name. }

var
  UserSize: Integer;

function SizeOf(Value: Integer): Integer;
begin
  SizeOf := Value + 1
end;

begin
  { The ordinary declaration shadows legacy System SizeOf. }
  UserSize := SizeOf(20);
  WriteLn(UserSize)
end.
