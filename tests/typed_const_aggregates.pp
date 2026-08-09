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

const
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
    Halt(26)
end.
