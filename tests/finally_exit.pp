program FinallyExit;

procedure P;
begin
  try
  finally
    Exit
  end
end;

begin
  P
end.
