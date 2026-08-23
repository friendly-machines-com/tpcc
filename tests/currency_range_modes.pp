program CurrencyRangeModes;

{$R-}
const
  HugeIntegerOrigin = 922337203685478;
  HugeRealOrigin = 922337203685477.5808;
  TypedHugeInteger: Int64 = 922337203685478;
  WrappedInteger: Currency = HugeIntegerOrigin;
  WrappedReal: Currency = HugeRealOrigin;
  WrappedTypedInteger: Currency = Currency(Int64(922337203685478));

var
  RuntimeInput: Int64;
  RuntimeWrapped: Currency;
  Sink: Currency;

procedure AcceptCurrency(Value: Currency);
begin
  Sink := Value
end;

procedure CompileUncheckedOrigin;
begin
  {$R-}
  AcceptCurrency(HugeIntegerOrigin)
end;

procedure CompileCheckedOrigin;
begin
  {$R+}
  AcceptCurrency(HugeIntegerOrigin)
end;

begin
{$ifdef EXECUTE_CHECKED}
  CompileCheckedOrigin
{$elseif defined(EXECUTE_CHECKED_TYPED)}
  RuntimeInput := TypedHugeInteger;
  {$R+}
  RuntimeWrapped := RuntimeInput
{$else}
  {$R-}
  RuntimeInput := TypedHugeInteger;
  RuntimeWrapped := RuntimeInput
{$endif}
end.
