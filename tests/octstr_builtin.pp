program OctStrBuiltin;

var
  L: LongInt;
  I: Int64;
  Q: QWord;
  A, B, C: ShortString;

begin
  L := 83;
  I := -1;
  Q := 8;
  A := OctStr(L, 5);
  B := OctStr(I, 4);
  C := OctStr(Q, 3)
end.
