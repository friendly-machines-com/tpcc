program BuiltinOverloadStr;

{ Same-scope global routines overload automatically. This nearer declaration
  does not implicitly open the outer legacy System name. }

var
  UserStrSeen: Integer;

procedure Str(Value: Integer);
begin
  UserStrSeen := Value
end;

begin
  { The ordinary declaration shadows legacy System Str. }
  Str(17);
  WriteLn(UserStrSeen)
end.
