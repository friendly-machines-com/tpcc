program CurrencyType;

const
  RealOrigin = 0.1;
  RealOriginAlias = RealOrigin;
  IntegerOrigin = 42;
  IntegerOriginAlias = IntegerOrigin;
  ExactTenth: Currency = 0.1;
  HalfToEvenZero: Currency = 0.00005;
  HalfToEvenTwo: Currency = 0.00015;
  FoldedSum: Currency = Currency(1.25) + Currency(2.5);
  FoldedProduct: Currency = Currency(1.2345) * Currency(2.0);

type
  PLocalInt64 = ^Int64;

var
  C: Currency;
  RuntimeSum: Currency;
  RuntimeProduct: Currency;
  FromInteger: Currency;
  FromIntegerAlias: Currency;
  FromReal: Currency;
  FromCall: Currency;
  AsDouble: Double;
  AsInteger: Int64;
  Quotient: Double;
  Raw: Int64;
  LiteralDomain: Integer;
  LiteralAliasDomain: Integer;
  TypedCurrencyDomain: Integer;
  CurrencyLess: Boolean;
  CurrencySize: SizeInt;
  ParsedExact: Currency;
  ParsedHalfEvenZero: Currency;
  ParsedHalfEvenTwo: Currency;
  ParsedExponent: Currency;
  ParsedMaximum: Currency;
  ParsedInvalid: Currency;
  ParsedOverflow: Currency;
  ParsedWithoutCode: Currency;
  ParsedAnsi: Currency;
  ParsedAnsiInput: AnsiString;
  FormattedRounded: AnsiString;
  FormattedHalf: AnsiString;
  FormattedMaximum: AnsiString;
  RoundTripText: string[64];
  RoundTripValue: Currency;
  ParsedExactCode: Integer;
  ParsedHalfEvenZeroCode: Integer;
  ParsedHalfEvenTwoCode: Integer;
  ParsedExponentCode: Integer;
  ParsedMaximumCode: Integer;
  ParsedInvalidCode: Integer;
  ParsedOverflowCode: Integer;
  ParsedAnsiCode: Word;
  RoundTripCode: Integer;

function Domain(Value: Single): Integer; overload;
begin
  Domain := 1
end;

function Domain(Value: Double): Integer; overload;
begin
  Domain := 2
end;

function Domain(Value: Extended): Integer; overload;
begin
  Domain := 3
end;

function Domain(Value: Currency): Integer; overload;
begin
  Domain := 4
end;

function IdentityCurrency(Value: Currency): Currency;
begin
  IdentityCurrency := Value
end;

procedure ExerciseCurrencyWrite;
begin
  { ncon.pas uses this exact source operation when dumping value_currency.
    TPCC resolves Write through the concrete System.Str Currency overload. }
  Writeln('value_currency = ', ParsedExact)
end;

begin
  { Two ordinary-real operands propose only an ordinary-real common domain.
    A Currency formal remains a valid destination context for either origin. }
  LiteralDomain := Domain(0.1 + 0.2);
  LiteralAliasDomain := Domain(RealOriginAlias + RealOrigin);
  TypedCurrencyDomain := Domain(ExactTenth);

  C := Currency(1.25);
  RuntimeSum := C + 2.5;
  RuntimeProduct := C * Currency(2.0);
  FromInteger := Integer(42);
  FromIntegerAlias := IdentityCurrency(IntegerOriginAlias);
  FromReal := Double(3.125);
  FromCall := IdentityCurrency(RealOriginAlias);
  AsDouble := C;
  AsInteger := Int64(C);
  Quotient := Currency(3.0) / Currency(2.0);
  CurrencyLess := Currency(1.0) < Currency(2.0);
  CurrencySize := SizeOf(Currency);

  Val('123.4567', ParsedExact, ParsedExactCode);
  Val('0.00005', ParsedHalfEvenZero, ParsedHalfEvenZeroCode);
  Val('0.00015', ParsedHalfEvenTwo, ParsedHalfEvenTwoCode);
  Val('-12.34565e1', ParsedExponent, ParsedExponentCode);
  Val('922337203685477.5807', ParsedMaximum, ParsedMaximumCode);
  Val('12x', ParsedInvalid, ParsedInvalidCode);
  Val('922337203685477.5808', ParsedOverflow, ParsedOverflowCode);
  Val('3.125', ParsedWithoutCode);
  ParsedAnsiInput := '-0.00015';
  Val(ParsedAnsiInput, ParsedAnsi, ParsedAnsiCode);
  Str(Currency(25.996):0:2, FormattedRounded);
  Str(Currency(1.005):0:2, FormattedHalf);
  Str(ParsedMaximum:0:4, FormattedMaximum);
  Str(ParsedMaximum, RoundTripText);
  Val(RoundTripText, RoundTripValue, RoundTripCode);

  { FPC's PPU serialization deliberately views Currency storage as Int64.
    This is a representation overlay, not a numeric Currency-to-Int64 cast. }
  Raw := PLocalInt64(@ExactTenth)^
end.
