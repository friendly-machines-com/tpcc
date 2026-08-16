program CurrencyNegativePrecision;

{$IFDEF CHECKED}
{$R+}
{$ELSE}
{$R-}
{$ENDIF}

var
  S: AnsiString;

begin
  { A negative precision is an invalid fixed-decimal format request.  It must
    fail independently of range checking because no numeric conversion is
    being narrowed here. }
  Str(Currency(1.0):0:-1, S)
end.
