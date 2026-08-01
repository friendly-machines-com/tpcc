program BuiltinOverloadTruncRound;

{ Same-scope global routines overload automatically. These nearer declarations
  do not implicitly open the outer ordinary System names. }

var
  UserTrunc: Int64;
  UserRound: Int64;

function Trunc(Value: Integer): Int64;
begin
  Trunc := Value + 1
end;

function Round(Value: Integer): Int64;
begin
  Round := Value + 2
end;

begin
  { Without `overload`, the nearer ordinary functions shadow the outer
    ordinary System functions. }
  UserTrunc := Trunc(20);
  UserRound := Round(20);

  WriteLn(UserTrunc);
  WriteLn(UserRound)
end.
