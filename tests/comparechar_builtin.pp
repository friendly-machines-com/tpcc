program CompareCharBuiltin;

var
  A: ShortString;
  B: ShortString;
  EqualResult: SizeInt;
  LessResult: SizeInt;
  GreaterResult: SizeInt;
  EmptyResult: SizeInt;
  ByteResult: SizeInt;

begin
  A := 'abc';
  B := 'abd';
  EqualResult := CompareChar(A[1], A[1], 3);
  LessResult := CompareChar(A[1], B[1], 3);
  GreaterResult := CompareChar(B[1], A[1], 3);
  EmptyResult := CompareChar(A[1], B[1], 0);
  ByteResult := CompareByte(A[1], B[1], 3)
end.
