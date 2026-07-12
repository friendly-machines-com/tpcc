program EmptyStatement;

var
  N: LongInt;

begin
  N := 0;

  if False then
  else
    N := N + 1;

  if True then;

  if True then
    begin
      if True then
    end;

  repeat
    if True then
  until True;

  if N <> 1 then
    begin
      N := 0;
      N := 1 div N
    end
end.
