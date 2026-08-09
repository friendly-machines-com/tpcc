program OperatorRankingPolicies;

type
  TPlanet = (Mercury, Venus, Earth);
  TFlag = record
    Value: Integer;
  end;
  TNumbers = array of Integer;
  TString10 = String[10];

var
  Planet, OtherPlanet: TPlanet;
  Amount: Integer;
  Flag, Inverted: TFlag;
  First, Alias, Other: TNumbers;
  Short: TString10;
  Managed, Combined: AnsiString;
  PowerInteger: Integer;
  PowerReal: Extended;

operator +(Left: TPlanet; Right: Integer): TPlanet;
begin
  if (Left = Left) and (Right = Right) then
    Result := Earth
end;

operator LogicalNot(Value: TFlag): TFlag;
begin
  Result.Value := not Value.Value
end;

begin
  Planet := Mercury;
  OtherPlanet := Venus;
  if not (Planet < OtherPlanet) then
    Halt(1);
  if Planet >= OtherPlanet then
    Halt(2);

  Amount := 1;
  Planet := Planet + Amount;
  if Planet <> Earth then
    Halt(3);

  Flag.Value := 0;
  Inverted := not Flag;
  if Inverted.Value <> -1 then
    Halt(4);

  SetLength(First, 1);
  Alias := First;
  SetLength(Other, 1);
  if First <> Alias then
    Halt(5);
  if First = Other then
    Halt(6);
  if First = nil then
    Halt(7);
  SetLength(First, 0);
  if First <> nil then
    Halt(8);

  Short := 'ab';
  Managed := 'cd';
  Combined := Short + Managed;
  if Combined <> 'abcd' then
    Halt(9);
  if not (Short < Managed) then
    Halt(10);
  Short := Short + 'c';
  if Short <> 'abc' then
    Halt(11);

  PowerInteger := 2 ** 10;
  if PowerInteger <> 1024 then
    Halt(12);
  PowerReal := 4.0 ** 0.5;
  if PowerReal <> 2.0 then
    Halt(13)
end.
