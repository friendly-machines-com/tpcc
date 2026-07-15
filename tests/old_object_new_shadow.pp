program old_object_new_shadow;

procedure New(Value: Integer);
begin
  WriteLn(Value)
end;

procedure Dispose(Value: Integer);
begin
  WriteLn(Value)
end;

begin
  New(31);
  Dispose(37)
end.
