program InsertBuiltin;

var
  S: ShortString;
  N: LongInt;

begin
  S := 'abcd';
  Insert('XY', S, 3);
  if S <> 'abXYcd' then
    begin
      N := 0;
      N := 1 div N
    end;

  Insert('0', S, 0);
  if S <> '0abXYcd' then
    begin
      N := 0;
      N := 1 div N
    end;

  Insert('Z', S, 99);
  if S <> '0abXYcdZ' then
    begin
      N := 0;
      N := 1 div N
    end;

  Insert('', S, 2);
  if S <> '0abXYcdZ' then
    begin
      N := 0;
      N := 1 div N
    end;

  S := 'abc';
  Insert(S, S, 2);
  if S <> 'aabcbc' then
    begin
      N := 0;
      N := 1 div N
    end
end.
