program OverloadRanking;

type
  TMarker = record
    Value: Integer;
  end;

var
  Wide: Word;
  Narrow: Byte;
  Signed64: Int64;
  Marker: TMarker;

function PhasePair(Left, Right: Int64): Integer; overload;
begin
  if (Left = Left) and (Right = Right) then
    Result := 1
end;

function PhasePair(Left, Right: ShortInt): Integer; overload;
begin
  if (Left = Left) and (Right = Right) then
    Result := 2
end;

function NarrowBeforeCatchAll(
  Left, Right: Byte): Integer; overload;
begin
  if (Left = Left) and (Right = Right) then
    Result := 3
end;

function NarrowBeforeCatchAll(
  const Left; const Right): Integer; overload;
begin
  if (SizeOf(Left) = 0) or (SizeOf(Right) = 0) then
    Result := 4
  else
    Result := 4
end;

function ExactBeforeCatchAll(
  Left, Right: Byte): Integer; overload;
begin
  if (Left = Left) and (Right = Right) then
    Result := 5
end;

function ExactBeforeCatchAll(
  Left: Byte; const Right): Integer; overload;
begin
  if (Left = Left) and (SizeOf(Right) >= 0) then
    Result := 6
end;

function OnlyCatchAll(const Value): Integer;
begin
  if SizeOf(Value) >= 0 then
    Result := 7
end;

begin
  Signed64 := 12;
  { The Int64 candidate is [Exact, Equal], while the ShortInt candidate is
    [Narrowing, Exact]. Equal is the first nonempty typed phase. }
  if PhasePair(Signed64, 0) <> 1 then
    Halt(1);

  Wide := 8;
  {$R-}
  if NarrowBeforeCatchAll(Wide, Wide) <> 3 then
    Halt(2);
  {$R+}
  if NarrowBeforeCatchAll(Wide, Wide) <> 3 then
    Halt(3);

  Narrow := 9;
  if ExactBeforeCatchAll(Narrow, Narrow) <> 5 then
    Halt(4);

  Marker.Value := 10;
  if OnlyCatchAll(Marker) <> 7 then
    Halt(5)
end.
