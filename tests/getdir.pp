program GetDirTest;

var
  Directory: ShortString;
  AnsiDirectory: AnsiString;
  Status: Word;

procedure ShowDirectory(Drive: Byte);
begin
  Directory := 'unchanged';
  GetDir(Drive, Directory);
  Status := IOResult;
  WriteLn(Directory);
  WriteLn(Status)
end;

procedure ShowAnsiDirectory(Drive: Byte);
begin
  AnsiDirectory := 'unchanged';
  GetDir(Drive, AnsiDirectory);
  Status := IOResult;
  WriteLn(AnsiDirectory);
  WriteLn(Status)
end;

begin
  if not ('/' in AllowDirectorySeparators) then
    Halt(1);
  if not ('\' in AllowDirectorySeparators) then
    Halt(2);
  if '.' in AllowDirectorySeparators then
    Halt(3);
  ShowDirectory(0);
  ShowDirectory(217);
  ShowAnsiDirectory(0);
  ShowAnsiDirectory(217)
end.
