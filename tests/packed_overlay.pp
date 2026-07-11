program PackedOverlay;

function ReverseWord(W: Word): Word;
type
  TWordRec = packed record
    Hi, Lo: Byte;
  end;
begin
  TWordRec(ReverseWord).Hi := TWordRec(W).Lo;
  TWordRec(ReverseWord).Lo := TWordRec(W).Hi
end;

function ReverseLongWord(L: LongWord): LongWord;
type
  TLongWordRec = packed record
    B: array[0..3] of Byte;
  end;
begin
  TLongWordRec(ReverseLongWord).B[0] := TLongWordRec(L).B[3];
  TLongWordRec(ReverseLongWord).B[1] := TLongWordRec(L).B[2];
  TLongWordRec(ReverseLongWord).B[2] := TLongWordRec(L).B[1];
  TLongWordRec(ReverseLongWord).B[3] := TLongWordRec(L).B[0]
end;

var
  N: LongInt;
  ReversedWord: Word;
  ReversedLongWord: LongWord;

begin
  ReversedWord := ReverseWord(4660);
  ReversedLongWord := ReverseLongWord(305419896);

  if ReversedWord <> 13330 then
    begin
      N := 0;
      N := 1 div N
    end;
  if ReversedLongWord <> 2018915346 then
    begin
      N := 0;
      N := 1 div N
    end
end.
