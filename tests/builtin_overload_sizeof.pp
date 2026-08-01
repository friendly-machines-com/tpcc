program BuiltinOverloadSizeOf;

{ Deliberately no `overload` directive: global routine families are
  automatically overloadable in this compiler. }

var
  UserSize: Integer;
  BuiltinSize: Integer;

function SizeOf(Value: Integer): Integer;
begin
  SizeOf := Value + 1
end;

begin
  { A value actual selects the user declaration. }
  UserSize := SizeOf(20);

  { A type operand selects the predefined SizeOf declaration. }
  BuiltinSize := SizeOf(Byte);

  WriteLn(UserSize);
  WriteLn(BuiltinSize)
end.
