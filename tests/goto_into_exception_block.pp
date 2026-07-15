program GotoIntoExceptionBlock;

label
  InsideTry;

begin
  goto InsideTry;
  try
  InsideTry:
  finally
  end
end.
