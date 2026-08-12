program SetStringBuiltin;

type
  TBuffer = array[0..5] of AnsiChar;
  TSmall = String[3];

var
  Buffer: TBuffer;
  S, Shared: AnsiString;
  Small: TSmall;

begin
  Buffer[0] := 'A';
  Buffer[1] := #0;
  Buffer[2] := 'B';
  Buffer[3] := 'C';
  Buffer[4] := 'D';
  Buffer[5] := 'E';

  SetString(S, @Buffer[0], 6);
  if Length(S) <> 6 then
    Halt(1);
  if (S[1] <> 'A') or
     (S[2] <> #0) or
     (S[3] <> 'B') or
     (S[6] <> 'E') then
    Halt(2);

  S := 'old value';
  SetString(S, @Buffer[2], 3);
  if S <> 'BCD' then
    Halt(3);

  S := 'abcdef';
  Shared := S;
  SetString(S, @S[2], 4);
  if S <> 'bcde' then
    Halt(4);
  if Shared <> 'abcdef' then
    Halt(5);

  S := 'nonempty';
  SetString(S, nil, 0);
  if Length(S) <> 0 then
    Halt(6);

  S := 'nonempty';
  SetString(S, nil, -1);
  if Length(S) <> 0 then
    Halt(7);

  SetString(S, nil, 3);
  if Length(S) <> 3 then
    Halt(8);

  SetString(Small, @Buffer[0], 6);
  if Length(Small) <> 3 then
    Halt(9);
  if (Small[1] <> 'A') or
     (Small[2] <> #0) or
     (Small[3] <> 'B') then
    Halt(10);

  Small := 'xyz';
  SetString(Small, nil, 2);
  if (Length(Small) <> 2) or
     (Small[1] <> 'x') or
     (Small[2] <> 'y') then
    Halt(11)
end.
