program ParamStrBuiltin;

var
  Argument: ShortString;
  Index: Integer;

begin
  if System.ParamCount <> 5 then
    Halt(8);
  if System.ParamStr(0) <> ParamStr(1) then
    Halt(1);
  if ParamStr(2) <> 'alpha' then
    Halt(2);
  if ParamStr(3) <> 'two words' then
    Halt(3);
  if ParamStr(4) <> '' then
    Halt(4);

  Argument := ParamStr(5);
  if Length(Argument) <> 255 then
    Halt(5);
  for Index := 1 to Length(Argument) do
    if Argument[Index] <> 'x' then
      Halt(6);

  if ParamStr(6) <> '' then
    Halt(7)
end.
