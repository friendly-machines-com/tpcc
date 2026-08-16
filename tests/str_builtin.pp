program StrBuiltin;

type
  TSmallText = string[32];
  TTinyText = string[6];
  TMixedEnum = (
    FirstName := -2,
    SparseName := 3
  );
  TMixedEnumRange = FirstName..SparseName;

const
  EnumAlias = SparseName;
  TextAlias = 'aliased';

var
  Value: Extended;
  Text: ShortString;
  DynamicText: AnsiString;
  IntegerValue: Integer;
  WideSigned: Int64;
  WideUnsigned: QWord;
  SmallText: TSmallText;
  TinyText: TTinyText;
  TextSource: TSmallText;
  CharacterValue: Char;
  PointerValue: PChar;
  PointerBacking: AnsiString;
  EnumValue: TMixedEnum;
  EnumRangeValue: TMixedEnumRange;

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
    Halt(10);

  EnumValue := FirstName;
  Str(EnumValue, SmallText);
  if SmallText <> 'FirstName' then
    Halt(11);

  { An untyped alias must format exactly like the member. Enum text obeys
    the same left-padded field-width rule as every other Str value. }
  Str(EnumAlias:12, SmallText);
  if SmallText <> '  SparseName' then
    Halt(12);

  Str(EnumAlias, DynamicText);
  if DynamicText <> 'SparseName' then
    Halt(13);

  EnumRangeValue := FirstName;
  Str(EnumRangeValue, SmallText);
  if SmallText <> 'FirstName' then
    Halt(14);

  Str(True:6, SmallText);
  if SmallText <> '  TRUE' then
    Halt(15);

  Str(Boolean(2), SmallText);
  if SmallText <> 'TRUE' then
    Halt(16);

  CharacterValue := 'Z';
  Str(CharacterValue, SmallText);
  if SmallText <> 'Z' then
    Halt(17);

  { Counted textual values retain embedded zero bytes. }
  TextSource := 'ab'#0'cd';
  Str(TextSource, DynamicText);
  if Length(DynamicText) <> 5 then
    Halt(18);
  if DynamicText[3] <> #0 then
    Halt(19);
  if DynamicText[5] <> 'd' then
    Halt(20);

  { The bounded destination applies after projection and therefore keeps the
    same prefix as ordinary ShortString assignment. }
  DynamicText := '1234567';
  Str(DynamicText, TinyText);
  if TinyText <> '123456' then
    Halt(21);

  { The source is fully projected before the destination is replaced. }
  DynamicText := 'self'#0'tail';
  Str(DynamicText, DynamicText);
  if Length(DynamicText) <> 9 then
    Halt(22);
  if DynamicText[5] <> #0 then
    Halt(23);
  if DynamicText[9] <> 'l' then
    Halt(24);

  { PChar is the zero-terminated textual projection. Its terminator is not
    copied, width pads the resulting sequence, and nil denotes empty text. }
  PointerBacking := 'pointer'#0'ignored';
  PointerValue := PChar(PointerBacking);
  Str(PointerValue:9, SmallText);
  if SmallText <> '  pointer' then
    Halt(25);
  PointerValue := nil;
  Str(PointerValue, DynamicText);
  if Length(DynamicText) <> 0 then
    Halt(26);

  { A textual literal and its untyped const alias have the same projection. }
  Str('literal', SmallText);
  if SmallText <> 'literal' then
    Halt(27);
  Str(TextAlias, SmallText);
  if SmallText <> 'aliased' then
    Halt(28);

  { Write and Str share the same FormattedValue lowering. }
  Writeln(EnumAlias:12)
end.
