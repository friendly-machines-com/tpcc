program WritableCastTemporaryRejected;

function GetChar: Char;
begin
  Result := #0
end;

begin
  Byte(GetChar()) := 1
end.
