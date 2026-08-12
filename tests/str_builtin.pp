program StrBuiltin;

type
  TSmallText = string[32];

var
  Value: Extended;
  Text: ShortString;
  DynamicText: AnsiString;
  IntegerValue: Integer;
  WideSigned: Int64;
  WideUnsigned: QWord;
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
    Halt(3);

  { TCmdStr in the FPC compiler is AnsiString. This is the same destination
    and real formatting form used by comphook.pas. }
  Str(Value:0:3, DynamicText);
  if DynamicText <> '1.500' then
    Halt(4);

  { Unlike every String[N] sink, AnsiString retains the complete field. }
  IntegerValue := 7;
  Str(IntegerValue:300, DynamicText);
  if Length(DynamicText) <> 300 then
    Halt(5);
  if DynamicText[1] <> ' ' then
    Halt(6);
  if DynamicText[299] <> ' ' then
    Halt(7);
  if DynamicText[300] <> '7' then
    Halt(8);

  WideSigned := -1234567890123;
  Str(WideSigned, DynamicText);
  if DynamicText <> '-1234567890123' then
    Halt(9);

  WideUnsigned := High(QWord);
  Str(WideUnsigned, DynamicText);
  if DynamicText <> '18446744073709551615' then
    Halt(10)
end.
