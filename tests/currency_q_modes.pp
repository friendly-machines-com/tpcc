program CurrencyQModes;

{$Q-}
const
  FoldedWrapped: Currency =
    Currency(922337203685477.5807) + Currency(0.0001);

var
  Maximum: Currency;
  Step: Currency;
  RuntimeWrapped: Currency;

begin
  Maximum := Currency(922337203685477.5807);
  Step := Currency(0.0001);
{$ifdef EXECUTE_CHECKED}
  {$Q+}
{$else}
  {$Q-}
{$endif}
  RuntimeWrapped := Maximum + Step
end.
