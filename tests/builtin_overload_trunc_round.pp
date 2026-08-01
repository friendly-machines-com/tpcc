program BuiltinOverloadTruncRound;

{ Deliberately no `overload` directives: global routine families are
  automatically overloadable in this compiler. }

var
  UserTrunc: Int64;
  UserRound: Int64;
  BuiltinTrunc: Int64;
  BuiltinRound: Int64;

function Trunc(Value: Integer): Int64;
begin
  Trunc := Value + 1
end;

function Round(Value: Integer): Int64;
begin
  Round := Value + 2
end;

begin
  { Integer actuals select the ordinary user routines. }
  UserTrunc := Trunc(20);
  UserRound := Round(20);

  { Real actuals select the predefined named operators. }
  BuiltinTrunc := Trunc(4.75);
  BuiltinRound := Round(4.75);

  WriteLn(UserTrunc);
  WriteLn(UserRound);
  WriteLn(BuiltinTrunc);
  WriteLn(BuiltinRound)
end.
