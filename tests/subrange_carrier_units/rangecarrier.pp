unit RangeCarrier;

interface

type
  TFirst = 1..5;
  TSecond = 1..5;
  TFirstAlias = TFirst;
  TShade = (ShadeDark, ShadeMid, ShadeLight);
  TShadeRange = ShadeDark..ShadeLight;
  TPackedRange = packed record
    Value: TFirst;
  end;

function Identify(Value: TFirst): Integer; overload;
function NextShade(Value: TShadeRange): TShadeRange;

implementation

function Identify(Value: TFirst): Integer;
begin
  if Value = Value then
    Result := 1
end;

function NextShade(Value: TShadeRange): TShadeRange;
begin
  Result := Value;
  Inc(Result)
end;

end.
