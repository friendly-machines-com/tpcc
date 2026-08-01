program BuiltinOverloadWrite;

{ Same-scope global routines overload automatically. These nearer declarations
  do not implicitly open the outer legacy System names. }

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
  { The ordinary declarations shadow legacy System Write/WriteLn. }
  Marker.Value := 11;
  Write(Marker);
  Marker.Value := 13;
  WriteLn(Marker);

  if UserWriteSeen <> 11 then
    Halt(1);
  if UserWriteLnSeen <> 13 then
    Halt(2)
end.
