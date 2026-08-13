program CompareCharBuiltin;

var
  A: ShortString;
  B: ShortString;
  EqualResult: SizeInt;
  LessResult: SizeInt;
  GreaterResult: SizeInt;
  EmptyResult: SizeInt;
  ByteResult: SizeInt;
  Bytes: array[0..3] of Byte;
  Words: array[0..2] of Word;
  OtherWords: array[0..2] of Word;
  ByteIndex: SizeInt;
  WordIndex: SizeInt;
  WordComparison: SizeInt;

begin
  A := 'abc';
  B := 'abd';
  Bytes[0] := 7;
  Bytes[1] := 9;
  Bytes[2] := 7;
  Bytes[3] := 0;
  Words[0] := $1234;
  Words[1] := $5678;
  Words[2] := $1234;
  OtherWords[0] := $1234;
  OtherWords[1] := $5679;
  OtherWords[2] := $1234;
  EqualResult := CompareChar(A[1], A[1], 3);
  LessResult := CompareChar(A[1], B[1], 3);
  GreaterResult := CompareChar(B[1], A[1], 3);
  EmptyResult := CompareChar(A[1], B[1], 0);
  ByteResult := CompareByte(A[1], B[1], 3);
  ByteIndex := IndexByte(Bytes[0], 4, 9);
  WordIndex := IndexWord(Words[0], 3, $5678);
  WordComparison := CompareWord(Words[0], OtherWords[0], 3)
end.
