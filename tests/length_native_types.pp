program LengthNativeTypes;

type
  TValues = array[3..6] of LongInt;

var
  S: ShortString;
  A: AnsiString;
  Values: TValues;
  B: Byte;
  PI: PtrInt;
  PU: PtrUInt;
  SI: SizeInt;
  SU: SizeUInt;

begin
  S := 'abc';
  A := S;
  B := Length(S);
  PI := Length(S);
  PU := Length(S);
  SI := Length(A);
  SI := Length(Values);
  SU := Length(S)
end.
