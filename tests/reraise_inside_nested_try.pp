program ReraiseInsideNestedTry;

uses
  SysUtils;

begin
  try
    raise Exception.Create('outer')
  except
    on Exception do
      begin
        try
        finally
          raise
        end
      end
  end
end.
