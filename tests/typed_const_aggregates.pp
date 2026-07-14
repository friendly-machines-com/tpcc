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

const
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
    Halt(13)
end.
