program BuiltinOverloadNewDispose;

{ Deliberately no `overload` directives: global routine families are
  automatically overloadable in this compiler. }

type
  PInteger = ^Integer;

var
  P: PInteger;

procedure New(Value: Integer);
begin
  WriteLn('user new ', Value)
end;

procedure Dispose(Value: Integer);
begin
  WriteLn('user dispose ', Value)
end;

begin
  { The Integer actuals select the user declarations. }
  New(31);

  { The pointer actuals select the implicit System lifecycle declarations,
    even though the user declarations remain in the same overload families. }
  New(P);
  P^ := 5;
  WriteLn(P^);
  Dispose(P);

  Dispose(37)
end.
