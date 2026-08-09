program RmDirTest;

uses
  SysUtils;

var
  ShortPath: ShortString;
  AnsiPath: AnsiString;
  Status: Word;
  Caught: Boolean;
  CaughtCode: Integer;

procedure Remember(E: EInOutError);
begin
  Caught := True;
  CaughtCode := E.ErrorCode
end;

begin
  {$I-}
  RmDir('');
  if IOResult <> 0 then
    Halt(1);

  ShortPath := 'short-empty';
  RmDir(ShortPath);
  if IOResult <> 0 then
    Halt(2);
  if DirectoryExists(ShortPath) then
    Halt(3);

  AnsiPath := 'ansi-empty';
  RmDir(AnsiPath);
  if IOResult <> 0 then
    Halt(4);
  if DirectoryExists(AnsiPath) then
    Halt(5);

  RmDir('missing');
  RmDir('pending-empty');
  Status := IOResult;
  if Status <> 2 then
    Halt(6);
  if not DirectoryExists('pending-empty') then
    Halt(7);
  RmDir('pending-empty');
  if IOResult <> 0 then
    Halt(8);

  RmDir('.');
  if IOResult <> 16 then
    Halt(9);

  RmDir('nonempty');
  if IOResult <> 5 then
    Halt(10);
  if not DirectoryExists('nonempty') then
    Halt(11);

  RmDir('regular-file');
  if IOResult <> 5 then
    Halt(12);

  {$I+}
  Caught := False;
  CaughtCode := -1;
  try
    RmDir('checked-missing')
  except
    on E: EInOutError do
      Remember(E)
  end;
  if not Caught then
    Halt(13);
  if CaughtCode <> 2 then
    Halt(14);
  if IOResult <> 0 then
    Halt(15);

  Caught := False;
  CaughtCode := -1;
  try
    RmDir('.')
  except
    on E: EInOutError do
      Remember(E)
  end;
  if not Caught then
    Halt(16);
  if CaughtCode <> 16 then
    Halt(17);
  if IOResult <> 0 then
    Halt(18)
end.
