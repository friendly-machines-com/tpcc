program LegacyBuiltinNoFallback;

procedure Write(Value: ShortString); overload;
begin
  if Length(Value) = 0 then
    Halt(1)
end;

begin
  Write('hello', 5)
end.
