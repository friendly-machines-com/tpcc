program OverloadRankingUntypedConstants;

const
  RegisterMemory = 32768;
  RegisterGpr = 2097152;

function IntegerKind(Value: Integer): Integer; overload;
begin
  if Value = Value then
    Result := 1
end;

function IntegerKind(Value: Cardinal): Integer; overload;
begin
  if Value = Value then
    Result := 2
end;

begin
  if IntegerKind(RegisterMemory or RegisterGpr) <> 1 then
    Halt(1)
end.
