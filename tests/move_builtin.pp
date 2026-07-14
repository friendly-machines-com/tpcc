program MoveBuiltin;

type
  TCharacters = array[0..5] of Char;

var
  A: TCharacters;
  S: AnsiString;
  P: PChar;

procedure ForwardMove(const Source; var Destination; Count: SizeInt);
begin
  Move(Source, Destination, Count)
end;

begin
  A[0] := 'a';
  A[1] := 'b';
  A[2] := 'c';
  A[3] := 'd';
  A[4] := 'e';
  A[5] := 'f';
  Move(A[0], A[1], 5);
  ForwardMove(A[0], A[1], 5);
  Move(A[0], A[1], 0);
  Move(A[0], A[1], -1);

  S := 'abcdef';
  Move(S[1], S[2], 4);

  GetMem(P, Length(S) + 1);
  Move(S[1], P[0], Length(S));
  P[Length(S)] := #0;
  FreeMem(P, Length(S) + 1)
end.
