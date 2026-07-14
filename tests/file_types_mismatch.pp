program FileTypesMismatch;

type
  TIntegerFile = file of Integer;
  TByteFile = file of Byte;

var
  Bytes: TByteFile;

procedure AcceptIntegerFile(var Value: TIntegerFile);
begin
end;

begin
  AcceptIntegerFile(Bytes);
end.
