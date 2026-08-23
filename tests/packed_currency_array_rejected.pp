program PackedCurrencyArrayRejected;

type
  TInvalidPackedCurrencyArray = packed record
    Values: array[0..1] of Currency;
  end;

var
  Value: TInvalidPackedCurrencyArray;

begin
  Value.Values[0] := Currency(1)
end.
