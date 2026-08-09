program OperatorRankingPolicyRejected;

type
  TPlanet = (Mercury, Venus, Earth);
  TNumbers = array of Integer;

var
  Planet: TPlanet;
  Character: Char;
  First, Second: TNumbers;

begin
{$ifdef ENUM_ARITHMETIC}
  Planet := Planet + 1;
{$endif}
{$ifdef ENUM_BITWISE}
  Planet := Planet and Planet;
{$endif}
{$ifdef ENUM_SHIFT}
  Planet := Planet shl 1;
{$endif}
{$ifdef CHAR_ARITHMETIC}
  Character := Character + 1;
{$endif}
{$ifdef ARRAY_CONCATENATION}
  First := First + Second;
{$endif}
end.
