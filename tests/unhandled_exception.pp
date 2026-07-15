program UnhandledException;

uses
  SysUtils;

begin
  raise Exception.Create('unhandled')
end.
