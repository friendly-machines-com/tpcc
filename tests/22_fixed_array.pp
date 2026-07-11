program p;

type
  TIndex = 1..3;
  TSmallArray = array[TIndex] of Integer;
  TNegativeArray = array[-2..0] of Integer;
  TColor = (Red, Green, Blue);
  TColorArray = array[Red..Blue] of Integer;
  TMatrix = array[1..2, 4..5] of Integer;

var
  a: TSmallArray;
  n: TNegativeArray;
  c: TColorArray;
  m: TMatrix;
  len: Integer;

begin
  a[1] := 10;
  n[-2] := 20;
  c[Green] := 30;
  m[2, 5] := 40;
  len := Length(a)
end.
