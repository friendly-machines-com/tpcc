program SubrangeCarriers;

uses RangeCarrier;

type
  TArray = array[2..4] of TFirst;
  TSmallSet = set of 1..5;

var
  First: TFirst;
  Second: TSecond;
  Alias: TFirstAlias;
  PackedValue: TPackedRange;
  Shade: TShadeRange;
  Values: TArray;
  SmallSet: TSmallSet;

function LocalCarrierValue: Integer;
type
  TLocal = -2..2;
var
  Local: TLocal;
begin
  Local := 2;
  Result := Integer(Ord(Local))
end;

begin
  First := 3;
  Second := 3;
  Alias := First;
  PackedValue.Value := Alias;
  Shade := TShadeRange(ShadeDark);
  Shade := NextShade(Shade);
  Values[2] := PackedValue.Value;
  SmallSet := [First];

  if Identify(First) <> 1 then
    Halt(1);
  if Identify(Second) <> 2 then
    Halt(2);
  if Identify(Alias) <> 1 then
    Halt(3);
  if Values[2] <> 3 then
    Halt(4);
  if not (3 in SmallSet) then
    Halt(5);
  if Ord(Shade) <> 1 then
    Halt(6);
  if LocalCarrierValue <> 2 then
    Halt(7);
  if SizeOf(TFirst) <> SizeOf(ShortInt) then
    Halt(8)
end.
