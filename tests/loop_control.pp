program LoopControl;

var
  I: LongInt;
  Sum: LongInt;
  N: LongInt;

begin
  I := 0;
  Sum := 0;
  while I < 10 do
    begin
      I := I + 1;
      if I = 2 then
        continue;
      if I = 5 then
        break;
      Sum := Sum + I
    end;
  if Sum <> 8 then
    begin
      N := 0;
      N := 1 div N
    end;

  I := 0;
  Sum := 0;
  repeat
    I := I + 1;
    if I = 2 then
      continue;
    if I = 5 then
      break;
    Sum := Sum + I
  until I >= 10;
  if Sum <> 8 then
    begin
      N := 0;
      N := 1 div N
    end;

  Sum := 0;
  for I := 1 to 10 do
    begin
      if I = 2 then
        continue;
      if I = 5 then
        break;
      Sum := Sum + I
    end;
  if Sum <> 8 then
    begin
      N := 0;
      N := 1 div N
    end;

  Sum := 0;
  for I := 5 downto 1 do
    begin
      if I = 4 then
        continue;
      if I = 2 then
        break;
      Sum := Sum + I
    end;
  if Sum <> 8 then
    begin
      N := 0;
      N := 1 div N
    end;

  N := 0;
  for I := High(LongInt) - 1 to High(LongInt) do
    begin
      N := N + 1;
      continue
    end;
  if N <> 2 then
    begin
      N := 0;
      N := 1 div N
    end
end.
