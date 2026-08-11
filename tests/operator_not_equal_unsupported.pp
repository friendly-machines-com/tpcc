program OperatorNotEqualUnsupported;

type
  TValue = record
    Number: Integer;
  end;

operator NotEqual(Left, Right: TValue): Boolean;
begin
  Result := Left.Number <> Right.Number
end;

begin
end.
