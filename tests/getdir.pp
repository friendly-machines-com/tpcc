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
  ShowDirectory(0);
  ShowDirectory(217);
  ShowAnsiDirectory(0);
  ShowAnsiDirectory(217)
end.
