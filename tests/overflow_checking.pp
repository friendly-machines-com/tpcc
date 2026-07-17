program OverflowChecking;

uses SysUtils;

var
  I: Integer;
  Caught: Boolean;

begin
  {$Q-}
  I := High(Integer);
  I := I + 1;
  if I <> Low(Integer) then
    Halt(1);
  I := Low(Integer);
  I := -I;
  if I <> Low(Integer) then
    Halt(2);
  I := Low(Integer);
  I := I div -1;
  if I <> Low(Integer) then
    Halt(3);
  I := Low(Integer);
  I := I - 1;
  if I <> High(Integer) then
    Halt(8);
  I := High(Integer);
  I := I * 2;
  if I <> -2 then
    Halt(9);

  {$Q+}
  Caught := False;
  try
    I := High(Integer);
    I := I + 1
  except
    on EIntOverflow do
      Caught := True
  end;
  if not Caught then
    Halt(4);

  Caught := False;
  try
    I := Low(Integer);
    I := -I
  except
    on EIntOverflow do
      Caught := True
  end;
  if not Caught then
    Halt(5);

  Caught := False;
  try
    I := Low(Integer);
    I := I div -1
  except
    on EIntOverflow do
      Caught := True
  end;
  if not Caught then
    Halt(6);

  Caught := False;
  try
    I := Low(Integer);
    I := I - 1
  except
    on EIntOverflow do
      Caught := True
  end;
  if not Caught then
    Halt(10);

  Caught := False;
  try
    I := High(Integer);
    I := I * 2
  except
    on EIntOverflow do
      Caught := True
  end;
  if not Caught then
    Halt(11);

  I := Low(Integer);
  if I mod -1 <> 0 then
    Halt(7)
end.
