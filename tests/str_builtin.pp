program StrBuiltin;

type
  TSmallText = string[32];

var
  Value: Extended;
  Text: ShortString;
  IntegerValue: Integer;
  SmallText: TSmallText;

begin
  Value := 3 / 2;
  Str(Value, Text);

  IntegerValue := 123;
  Str(IntegerValue, SmallText);
  if SmallText <> '123' then
    Halt(1);

  Str(IntegerValue:6, SmallText);
  if SmallText <> '   123' then
    Halt(2)
end.
