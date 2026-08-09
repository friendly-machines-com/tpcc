program ChangeFileExtTest;

uses
  SysUtils;

var
  Source, ResultValue: AnsiString;
  I: SizeInt;

begin
  if ChangeFileExt('file.pas', '.o') <> 'file.o' then
    Halt(1);
  if ChangeFileExt('file', '.o') <> 'file.o' then
    Halt(2);
  if ChangeFileExt('file.', '.o') <> 'file.o' then
    Halt(3);
  if ChangeFileExt('file.pas', '') <> 'file' then
    Halt(4);
  if ChangeFileExt('file.pas', 'o') <> 'fileo' then
    Halt(5);

  if ChangeFileExt('/tmp.with.dot/file.pas', '.o') <>
      '/tmp.with.dot/file.o' then
    Halt(6);
  if ChangeFileExt('/tmp.with.dot/file', '.o') <>
      '/tmp.with.dot/file.o' then
    Halt(7);
  if ChangeFileExt('dir.with.dot\file.pas', '.o') <>
      'dir.with.dot\file.o' then
    Halt(8);
  if ChangeFileExt('/tmp/', '.o') <> '/tmp/.o' then
    Halt(9);

  if ChangeFileExt('.profile', '.bak') <> '.profile.bak' then
    Halt(10);
  if ChangeFileExt('/tmp/.profile', '.bak') <>
      '/tmp/.profile.bak' then
    Halt(11);
  if ChangeFileExt('/tmp/.profile.local', '.bak') <>
      '/tmp/.profile.bak' then
    Halt(12);

  if ChangeFileExt('', '.o') <> '.o' then
    Halt(13);
  if ChangeFileExt('', '') <> '' then
    Halt(14);

  Source := '';
  for I := 1 to 400 do
    Source := Source + 'x';
  Source := Source + '.old';
  ResultValue := ChangeFileExt(Source, '.new');
  if Length(ResultValue) <> 404 then
    Halt(15);
  for I := 1 to 400 do
    if ResultValue[I] <> 'x' then
      Halt(16);
  if Copy(ResultValue, 401, 4) <> '.new' then
    Halt(17)
end.
