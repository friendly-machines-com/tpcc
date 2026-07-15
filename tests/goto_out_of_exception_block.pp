program GotoOutOfExceptionBlock;

label
  OutsideTry;

begin
  try
    goto OutsideTry
  finally
  end;
OutsideTry:
end.
