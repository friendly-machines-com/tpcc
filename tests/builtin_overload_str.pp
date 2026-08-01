program BuiltinOverloadStr;

{ Deliberately no `overload` directive: global routine families are
  automatically overloadable in this compiler. }

var
  UserStrSeen: Integer;
  BuiltinValue: Int64;
  Text: ShortString;

procedure Str(Value: Integer);
begin
  UserStrSeen := Value
end;

begin
  { One actual selects the user declaration. }
  Str(17);

  { The destination actual selects the predefined Str declaration. }
  BuiltinValue := 123;
  Str(BuiltinValue, Text);

  WriteLn(UserStrSeen);
  WriteLn(Text)
end.
