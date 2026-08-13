program WritableByteArrayTemporaryRejected;

type
  TDoubleBytes = array[0..7] of Byte;

function GetValue: Double;
begin
  Result := 1.5
end;

begin
  TDoubleBytes(GetValue)[0] := 0
end.
