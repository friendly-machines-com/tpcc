program LengthNativeTypes;

type
  TValues = array[3..6] of LongInt;

var
  S: ShortString;
  A: AnsiString;
  Values: TValues;
  PI: PtrInt;
  PU: PtrUInt;
  SI: SizeInt;
  SU: SizeUInt;

begin
  S := 'abc';
  A := S;
  PI := Length(S);
  PU := Length(A);
  SI := Length(Values);
  SU := Length(S)
end.
