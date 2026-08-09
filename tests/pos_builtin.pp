program PosBuiltin;

const
  FoldedFound = Pos('bc', 'abcd');
  FoldedMissing = Pos('xy', 'abcd');
  FoldedEmpty = Pos('', 'abcd');

var
  S: ShortString;
  N: LongInt;

begin
  S := 'a'#0'b';
  if Length(S) <> 3 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Ord(S[2]) <> 0 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Ord(#13) <> 13 then
    begin
      N := 0;
      N := 1 div N
    end;

  if FoldedFound <> 2 then
    begin
      N := 0;
      N := 1 div N
    end;
  if FoldedMissing <> 0 then
    begin
      N := 0;
      N := 1 div N
    end;
  if FoldedEmpty <> 0 then
    begin
      N := 0;
      N := 1 div N
    end;

  S := 'abracadabra';
  if Pos('cad', S) <> 5 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Pos('xyz', S) <> 0 then
    begin
      N := 0;
      N := 1 div N
    end
end.
