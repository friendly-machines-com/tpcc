program custom_conversion_constant_rejected;

type
  TBox = type Integer;

operator :=(Value: Integer): TBox;
begin
  Result := TBox(Value)
end;

const
  Folded: TBox = 1 + 2;

begin
end.
