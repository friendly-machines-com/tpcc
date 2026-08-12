program DeleteFileTest;

uses
  SysUtils;

begin
  if not SysUtils.DeleteFile('regular') then
    Halt(1);
  if SysUtils.FileExists('regular', False) then
    Halt(2);

  if SysUtils.DeleteFile('missing') then
    Halt(3);
  if SysUtils.DeleteFile('') then
    Halt(4);

  if not SysUtils.DeleteFile('file-link') then
    Halt(5);
  if not SysUtils.FileExists('target') then
    Halt(6);

  if not SysUtils.DeleteFile('broken-link') then
    Halt(7);

  if SysUtils.DeleteFile('directory') then
    Halt(8);
  if not SysUtils.DirectoryExists('directory') then
    Halt(9)
end.
