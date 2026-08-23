program CurrencyCommonDomainAmbiguous;

const
  LossyOrigin = 0.00001;
  LossyOriginAlias = LossyOrigin;

var
  C: Currency;
  I64: Int64;
  Q: QWord;
  D: Double;
  E: Extended;
  B: Boolean;

begin
  C := C + I64;
  C := I64 + C;
  C := C + Q;
  C := Q + C;
  D := C + D;
  D := D + C;
  E := C + E;
  E := E + C;
  C := C + 0.00001;
  C := 0.00001 + C;
  C := C + LossyOriginAlias;
  C := LossyOriginAlias + C;
  B := C < LossyOriginAlias;
  B := LossyOriginAlias < C;
  B := C < I64;
  B := I64 < C;
  B := C < D;
  B := D < C
end.
