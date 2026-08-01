program BuiltinOverloadNewDispose;

{ Same-scope global routines overload automatically. This nearer declaration
  does not implicitly open the outer legacy System name. }

procedure New(Value: Integer);
begin
  WriteLn('user new ', Value)
end;

procedure Dispose(Value: Integer);
begin
  WriteLn('user dispose ', Value)
end;

begin
  { A nearer ordinary declaration selects the new-Pascal layer. The legacy
    System declarations are not fallback overloads. }
  New(31);
  Dispose(37)
end.
