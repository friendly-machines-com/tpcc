program OverloadRankingSignednessAmbiguous;

var
  Left, Right: Byte;
  Value: Integer;

function Pick(Left: Integer; Right: Cardinal): Integer; overload;
begin
  Result := Left + Integer(Right)
end;

function Pick(Left: Cardinal; Right: Integer): Integer; overload;
begin
  Result := Integer(Left) + Right
end;

begin
  Value := Pick(Left, Right)
end.
