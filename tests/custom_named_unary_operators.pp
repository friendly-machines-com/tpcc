program CustomNamedUnaryOperators;

type
  TValue = record
    Data: Integer;
  end;

operator Trunc(const Value: TValue): Integer;
begin
  Result := Value.Data + 10
end;

operator Round(const Value: TValue): Integer;
begin
  Result := Value.Data + 20
end;

{ This ordinary routine deliberately has the same source spelling and
  signature as the operator. Trunc(...) syntax must select only the canonical
  operator family; it must not merge ordinary and operator candidates. }
function Trunc(const Value: TValue): Integer;
begin
  Result := Value.Data - Value.Data - 1
end;

var
  Value: TValue;
begin
  Value.Data := 7;
  if Trunc(Value) <> 17 then
    Halt(1);
  if Round(Value) <> 27 then
    Halt(2)
end.
