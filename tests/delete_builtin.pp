program DeleteBuiltin;

var
  S: ShortString;
  N: LongInt;

begin
  S := 'abracadabra';
  Delete(S, 4, 3);
  if Length(S) <> 8 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Pos('d', S) <> 4 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Pos('c', S) <> 0 then
    begin
      N := 0;
      N := 1 div N
    end;

  Delete(S, 5, 100);
  if Length(S) <> 4 then
    begin
      N := 0;
      N := 1 div N
    end;

  Delete(S, 1, 0);
  Delete(S, 99, 1);
  if Length(S) <> 4 then
    begin
      N := 0;
      N := 1 div N
    end
end.
