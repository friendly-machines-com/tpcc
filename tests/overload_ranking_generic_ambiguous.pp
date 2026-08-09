program OverloadRankingGenericAmbiguous;

var
  Left, Right: Byte;
  Value: Integer;

function Pick(
  Left: Byte; const Right): Integer; overload;
begin
  Result := 1
end;

function Pick(
  const Left; Right: Byte): Integer; overload;
begin
  Result := 2
end;

begin
  Value := Pick(Left, Right)
end.
