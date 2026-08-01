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
    Halt(2);

  { Selection of Int64 rather than QWord belongs to ordinary overload
    ranking; predefined Str must not need its own literal rule. }
  Str(42, SmallText);
  if SmallText <> '42' then
    Halt(3)
end.
