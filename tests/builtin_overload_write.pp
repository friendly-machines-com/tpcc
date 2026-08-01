program BuiltinOverloadWrite;

{ Deliberately no `overload` directives: global routine families are
  automatically overloadable in this compiler. }

type
  TMarker = record
    Value: Integer
  end;

var
  Marker: TMarker;
  UserWriteSeen: Integer;
  UserWriteLnSeen: Integer;

procedure Write(Value: TMarker);
begin
  UserWriteSeen := Value.Value
end;

procedure WriteLn(Value: TMarker);
begin
  UserWriteLnSeen := Value.Value
end;

begin
  { These select the user declarations, not the variadic System grammar. }
  Marker.Value := 11;
  Write(Marker);
  Marker.Value := 13;
  WriteLn(Marker);

  { These select the System declarations from the same overload families. }
  Write('builtin');
  WriteLn(' output');
  WriteLn(UserWriteSeen);
  WriteLn(UserWriteLnSeen)
end.
