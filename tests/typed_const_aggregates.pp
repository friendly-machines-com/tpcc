program TypedConstAggregates;

type
  TFlag = (FlagA, FlagB, FlagC);
  TFlags = set of TFlag;
  TNumbers = array[1..3] of LongInt;
  TInner = record
    Id: Byte;
    Numbers: TNumbers;
    Flags: TFlags;
  end;
  TInnerArray = array[0..1] of TInner;
  TOuter = record
    Items: TInnerArray;
    Extra: TFlags;
  end;
  TSetArray = array[1..2] of TFlags;
  TPackedValue = packed record
    Code: Byte;
    Value: LongInt;
  end;
  TPackedArray = array[1..2] of TPackedValue;
  TPartial = record
    Present: LongInt;
    Omitted: LongInt;
  end;
  TManaged = record
    Text: AnsiString;
    Embedded: AnsiString;
    LongText: AnsiString;
    Narrowed: String[3];
  end;
  THexTable = array[0..15] of Char;
  TChar4 = array[1..4] of Char;
  TCharRows = array[0..1] of TChar4;
  TCharacterRecord = record
    Kind: Byte;
    Text: TChar4;
  end;

const
  CharacterAlias = 'xy';
  HexTable: THexTable = '0123456789abcdef';
  CharacterAliasArray: TChar4 = CharacterAlias;
  EmbeddedCharacterArray: TChar4 = 'A'#0;
  EmptyCharacterArray: TChar4 = '';
  CharacterFunctionArray: TChar4 = Chr(66);
  CharacterCastArray: TChar4 = Char(67);
  CharacterRows: TCharRows = (
    'ab',
    'wxyz'
  );
  CharacterRecord: TCharacterRecord = (
    Kind: 7;
    Text: 'R'
  );
  LongManaged =
    AnsiString('01234567890123456789012345678901234567890123456789012345678901234567890123456789') +
    AnsiString('01234567890123456789012345678901234567890123456789012345678901234567890123456789') +
    AnsiString('01234567890123456789012345678901234567890123456789012345678901234567890123456789') +
    AnsiString('01234567890123456789012345678901234567890123456789012345678901234567890123456789');
  Outer: TOuter = (
    Items: (
      (Id: 7; Numbers: (10, 20, 30); Flags: [FlagA, FlagC]),
      (Id: 8; Numbers: (40, 50, 60); Flags: [])
    );
    Extra: [FlagB..FlagC]
  );
  SetArray: TSetArray = (
    [FlagA],
    [FlagB..FlagC]
  );
  PackedValues: TPackedArray = (
    (Code: 1; Value: 100),
    (Code: 2; Value: 200)
  );
  Partial: TPartial = (
    Present: 9
  );
  Managed: TManaged = (
    { Add remains a ShortString-valued operation here. Assigning its constant
      result selects System's unary ShortString -> AnsiString conversion. }
    Text: 'folded implicit ' + 'string conversion';
    Embedded: 'A'#0'BCD';
    LongText: LongManaged;
    { Add is AnsiString-valued, so this exercises the compiler-defined
      narrowing conversion rather than contextual literal construction. }
    Narrowed: AnsiString('abc') + AnsiString('def')
  );

function NextCounter: LongInt;
const
  Counter: LongInt = 0;
begin
  Counter := Counter + 1;
  Result := Counter
end;

var
  OuterAddress: ^TOuter;
  RuntimeShort: ShortString;
  RuntimeAnsi: AnsiString;
  RuntimeNarrowed: String[3];

begin
  if Outer.Items[0].Id <> 7 then
    Halt(1);
  if Outer.Items[0].Numbers[3] <> 30 then
    Halt(2);
  if not (FlagA in Outer.Items[0].Flags) then
    Halt(3);
  if FlagB in Outer.Items[0].Flags then
    Halt(4);
  if Outer.Items[1].Id <> 8 then
    Halt(5);
  if Outer.Items[1].Numbers[1] <> 40 then
    Halt(6);
  if FlagA in Outer.Items[1].Flags then
    Halt(7);
  if not (FlagB in Outer.Extra) then
    Halt(8);
  if not (FlagC in SetArray[2]) then
    Halt(9);
  if PackedValues[1].Code <> 1 then
    Halt(10);
  if PackedValues[2].Value <> 200 then
    Halt(11);
  if Partial.Present <> 9 then
    Halt(12);
  if Partial.Omitted <> 0 then
    Halt(13);
  OuterAddress := @Outer;
  Outer.Items[0].Id := 9;
  if OuterAddress^.Items[0].Id <> 9 then
    Halt(14);
  if NextCounter <> 1 then
    Halt(15);
  if NextCounter <> 2 then
    Halt(16);
  if Managed.Text <> 'folded implicit string conversion' then
    Halt(17);
  if Length(Managed.Embedded) <> 5 then
    Halt(18);
  if (Managed.Embedded[1] <> 'A') or
     (Ord(Managed.Embedded[2]) <> 0) or
     (Managed.Embedded[5] <> 'D') then
    Halt(19);
  if Length(Managed.LongText) <> 320 then
    Halt(20);
  if (Managed.LongText[256] <> '5') or
     (Managed.LongText[320] <> '9') then
    Halt(21);
  if Managed.Narrowed <> 'abc' then
    Halt(22);

  RuntimeShort := 'folded implicit ' + 'string conversion';
  RuntimeAnsi := RuntimeShort;
  if RuntimeAnsi <> Managed.Text then
    Halt(23);

  RuntimeAnsi := 'A'#0'BCD';
  if RuntimeAnsi <> Managed.Embedded then
    Halt(24);

  RuntimeAnsi :=
    AnsiString('01234567890123456789012345678901234567890123456789012345678901234567890123456789') +
    AnsiString('01234567890123456789012345678901234567890123456789012345678901234567890123456789') +
    AnsiString('01234567890123456789012345678901234567890123456789012345678901234567890123456789') +
    AnsiString('01234567890123456789012345678901234567890123456789012345678901234567890123456789');
  if RuntimeAnsi <> Managed.LongText then
    Halt(25);

  RuntimeNarrowed := AnsiString('abc') + AnsiString('def');
  if RuntimeNarrowed <> Managed.Narrowed then
    Halt(26);

  if (HexTable[0] <> '0') or (HexTable[15] <> 'f') then
    Halt(27);
  if (CharacterAliasArray[1] <> 'x') or
     (CharacterAliasArray[2] <> 'y') or
     (CharacterAliasArray[3] <> #0) or
     (CharacterAliasArray[4] <> #0) then
    Halt(28);
  if (EmbeddedCharacterArray[1] <> 'A') or
     (EmbeddedCharacterArray[2] <> #0) or
     (EmbeddedCharacterArray[3] <> #0) or
     (EmbeddedCharacterArray[4] <> #0) then
    Halt(29);
  if (EmptyCharacterArray[1] <> #0) or
     (EmptyCharacterArray[4] <> #0) then
    Halt(30);
  if (CharacterFunctionArray[1] <> 'B') or
     (CharacterFunctionArray[2] <> #0) then
    Halt(31);
  if (CharacterCastArray[1] <> 'C') or
     (CharacterCastArray[2] <> #0) then
    Halt(32);
  if (CharacterRows[0][1] <> 'a') or
     (CharacterRows[0][2] <> 'b') or
     (CharacterRows[0][3] <> #0) or
     (CharacterRows[1][4] <> 'z') then
    Halt(33);
  if (CharacterRecord.Kind <> 7) or
     (CharacterRecord.Text[1] <> 'R') or
     (CharacterRecord.Text[2] <> #0) then
    Halt(34)
end.
