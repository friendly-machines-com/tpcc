program TryStatements;

var
  State: LongInt;
  I: LongInt;
  S: string[1];

procedure Fail;
var
  Zero: LongInt;
begin
  Zero := 0;
  I := 1 div Zero
end;

procedure LeaveEarly(var Value: LongInt);
begin
  try
    Value := 1;
    Exit
  finally
    Value := 2
  end;
  Value := 99
end;

begin
  State := 0;
  try
    State := 1
  except
    State := 99
  end;
  if State <> 1 then
    Fail;

  S := '';
  try
    S[2] := 'x';
    State := 99
  except
    State := 3
  end;
  if State <> 3 then
    Fail;

  State := 0;
  try
    try
      S[2] := 'x'
    finally
      State := 4
    end
  except
    State := State + 1
  end;
  if State <> 5 then
    Fail;

  State := 0;
  try
    State := 6
  finally
    State := State + 1
  end;
  if State <> 7 then
    Fail;

  LeaveEarly(State);
  if State <> 2 then
    Fail;

  State := 0;
  while true do
    begin
      try
        break
      finally
        State := 8
      end
    end;
  if State <> 8 then
    Fail;

  I := 0;
  State := 0;
  while I < 3 do
    begin
      I := I + 1;
      try
        continue
      finally
        State := State + 1
      end;
      State := 99
    end;
  if State <> 3 then
    Fail;

  State := 0;
  try
    State := 9
  finally
    while true do
      begin
        State := State + 1;
        break
      end
  end;
  if State <> 10 then
    Fail;

  State := 0;
  try
    try
      State := 11
    finally
      S[2] := 'x'
    end
  except
    State := State + 1
  end;
  if State <> 12 then
    Fail
end.
