program FileTypesIdentityMismatch;

type
  TIntegerFile = file of Integer;
  TOtherIntegerFile = file of Integer;

var
  Other: TOtherIntegerFile;

procedure AcceptIntegerFile(var Value: TIntegerFile);
begin
end;

begin
  AcceptIntegerFile(Other)
end.
