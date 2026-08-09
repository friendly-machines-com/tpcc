program FindFirstFindNext;

uses
  SysUtils;

var
  Search: TRawByteSearchRec;
  Code, Count: LongInt;
  FoundOne, FoundTwo: Boolean;
  LastName: AnsiString;
  CurrentDirectory: AnsiString;
  ParentDirectory: AnsiString;

begin
  GetDir(0, CurrentDirectory);
  if IOResult <> 0 then
    Halt(88);
  if SysUtils.ExpandFileName('') <> CurrentDirectory + '/' then
    Halt(89);
  if SysUtils.ExpandFileName('.') <> CurrentDirectory then
    Halt(90);
  if SysUtils.ExpandFileName('foo') <> CurrentDirectory + '/foo' then
    Halt(91);
  if SysUtils.ExpandFileName('foo/') <> CurrentDirectory + '/foo/' then
    Halt(92);
  if SysUtils.ExpandFileName('foo/.') <> CurrentDirectory + '/foo' then
    Halt(93);
  if SysUtils.ExpandFileName('foo/..') <> CurrentDirectory then
    Halt(94);
  if SysUtils.ExpandFileName('foo/../bar') <>
      CurrentDirectory + '/bar' then
    Halt(95);
  if SysUtils.ExpandFileName('/') <> '/' then
    Halt(96);
  if SysUtils.ExpandFileName('/foo//./bar') <> '/foo/bar' then
    Halt(97);
  if SysUtils.ExpandFileName('/foo/../bar') <> '/bar' then
    Halt(98);
  if SysUtils.ExpandFileName('/../../bar') <> '/bar' then
    Halt(99);
  if SysUtils.ExpandFileName('\foo\bar') <> '/foo/bar' then
    Halt(100);
  if SysUtils.ExpandFileName('//foo///bar') <> '//foo/bar' then
    Halt(101);
  if SysUtils.ExpandFileName('~') <> CurrentDirectory + '/home' then
    Halt(102);
  if SysUtils.ExpandFileName('~/file') <>
      CurrentDirectory + '/home/file' then
    Halt(103);
  if SysUtils.ExpandFileName('~someone/file') <>
      CurrentDirectory + '/~someone/file' then
    Halt(104);
  if SysUtils.ExpandFileName('C:\foo') <>
      CurrentDirectory + '/C:/foo' then
    Halt(105);
  if SysUtils.ExpandFileName('missing/../still-missing') <>
      CurrentDirectory + '/still-missing' then
    Halt(106);
  ParentDirectory := ExtractFilePath(CurrentDirectory);
  if Length(ParentDirectory) > 1 then
    ParentDirectory := Copy(ParentDirectory, 1,
      Length(ParentDirectory) - 1);
  if SysUtils.ExpandFileName('..') <> ParentDirectory then
    Halt(107);
  if SysUtils.ExpandFileName('../') <> ParentDirectory + '/' then
    Halt(108);
  if SysUtils.ExpandFileName('//') <> '//' then
    Halt(109);
  if SysUtils.ExpandFileName('///') <> '//' then
    Halt(110);
  if SysUtils.ExpandFileName('//foo/..') <> '/' then
    Halt(111);
  if SysUtils.ExpandFileName('//foo/../') <> '//' then
    Halt(112);
  if SysUtils.ExpandFileName('//../bar') <> '/bar' then
    Halt(113);
  if SysUtils.ExpandFileName('...') <>
      CurrentDirectory + '/...' then
    Halt(114);
  if SysUtils.ExpandFileName('./') <> CurrentDirectory + '/' then
    Halt(115);
  if SysUtils.ExpandFileName('a//b///') <>
      CurrentDirectory + '/a/b/' then
    Halt(116);
  if SysUtils.ExpandFileName('a/.../b') <>
      CurrentDirectory + '/a/.../b' then
    Halt(117);

  if not SysUtils.FileExists('files/alpha1.dat') then
    Halt(73);
  if not SysUtils.FileExists('files/alpha1.dat', False) then
    Halt(74);
  if SysUtils.FileExists('') then
    Halt(75);
  if SysUtils.FileExists('files/missing') then
    Halt(76);
  if SysUtils.FileExists('files/missing', False) then
    Halt(77);
  if SysUtils.FileExists('files/subdir') then
    Halt(78);
  if SysUtils.FileExists('files/subdir', False) then
    Halt(79);
  if not SysUtils.FileExists('files/file-link') then
    Halt(80);
  if not SysUtils.FileExists('files/file-link', False) then
    Halt(81);
  if SysUtils.FileExists('files/dir-link') then
    Halt(82);
  if SysUtils.FileExists('files/dir-link', False) then
    Halt(83);
  if SysUtils.FileExists('files/broken-link') then
    Halt(84);
  if not SysUtils.FileExists('files/broken-link', False) then
    Halt(85);
  if not SysUtils.FileExists('files/pipe') then
    Halt(86);
  if not SysUtils.FileExists('files/pipe', False) then
    Halt(87);
  if not SysUtils.DirectoryExists('files/subdir') then
    Halt(118);
  if not SysUtils.DirectoryExists('files/subdir', False) then
    Halt(119);
  if SysUtils.DirectoryExists('files/alpha1.dat') then
    Halt(120);
  if SysUtils.DirectoryExists('files/alpha1.dat', False) then
    Halt(121);
  if SysUtils.DirectoryExists('files/file-link') then
    Halt(122);
  if SysUtils.DirectoryExists('files/file-link', False) then
    Halt(123);
  if not SysUtils.DirectoryExists('files/dir-link') then
    Halt(124);
  if not SysUtils.DirectoryExists('files/dir-link', False) then
    Halt(125);
  if SysUtils.DirectoryExists('files/broken-link') then
    Halt(126);
  if not SysUtils.DirectoryExists('files/broken-link', False) then
    Halt(127);
  if SysUtils.DirectoryExists('files/missing') then
    Halt(128);
  if SysUtils.DirectoryExists('files/missing', False) then
    Halt(129);
  if SysUtils.DirectoryExists('') then
    Halt(130);
  if SysUtils.DirectoryExists('', False) then
    Halt(131);
  if SysUtils.DirectoryExists('files/pipe') then
    Halt(132);
  if SysUtils.DirectoryExists('files/pipe', False) then
    Halt(133);

  if IncludeTrailingPathDelimiter('') <> '/' then
    Halt(50);
  if IncludeTrailingPathDelimiter('files') <> 'files/' then
    Halt(51);
  if IncludeTrailingPathDelimiter('files/') <> 'files/' then
    Halt(52);
  if IncludeTrailingPathDelimiter('/') <> '/' then
    Halt(53);
  if IncludeTrailingPathDelimiter('files\') <> 'files\' then
    Halt(54);

  if ExtractFilePath('') <> '' then
    Halt(55);
  if ExtractFileName('') <> '' then
    Halt(56);
  if ExtractFilePath('plain.dat') <> '' then
    Halt(57);
  if ExtractFileName('plain.dat') <> 'plain.dat' then
    Halt(58);
  if ExtractFilePath('files/alpha1.dat') <> 'files/' then
    Halt(59);
  if ExtractFileName('files/alpha1.dat') <> 'alpha1.dat' then
    Halt(60);
  if ExtractFilePath('/files/alpha1.dat') <> '/files/' then
    Halt(61);
  if ExtractFileName('/files/alpha1.dat') <> 'alpha1.dat' then
    Halt(62);
  if ExtractFilePath('/') <> '/' then
    Halt(63);
  if ExtractFileName('/') <> '' then
    Halt(64);
  if ExtractFilePath('files/subdir/') <> 'files/subdir/' then
    Halt(65);
  if ExtractFileName('files/subdir/') <> '' then
    Halt(66);
  if ExtractFilePath('files//name') <> 'files//' then
    Halt(67);
  if ExtractFileName('files//name') <> 'name' then
    Halt(68);
  if ExtractFilePath('files\name') <> 'files\' then
    Halt(69);
  if ExtractFileName('files\name') <> 'name' then
    Halt(70);
  if ExtractFilePath('drive:name') <> '' then
    Halt(71);
  if ExtractFileName('drive:name') <> 'drive:name' then
    Halt(72);

  Code := FindFirst('files/alpha1.dat', faAnyFile, Search);
  if Code <> 0 then
    Halt(1);
  if Search.Name <> 'alpha1.dat' then
    Halt(2);
  if Search.Size <> 3 then
    Halt(3);
  if Search.Time <= 0 then
    Halt(4);
  if Search.Mode = 0 then
    Halt(5);
  if (Search.Attr and faArchive) = 0 then
    Halt(6);
  if (Search.Attr and faDirectory) <> 0 then
    Halt(7);
  if Search.ExcludeAttr <> 0 then
    Halt(8);
  if Search.FindHandle = nil then
    Halt(9);
  if FindNext(Search) <> -1 then
    Halt(10);
  if Search.Name <> 'alpha1.dat' then
    Halt(11);
  FindClose(Search);
  if Search.FindHandle <> nil then
    Halt(12);
  FindClose(Search);

  Count := 0;
  FoundOne := False;
  FoundTwo := False;
  Code := FindFirst('files/alpha?.dat',
    faAnyFile and not faDirectory, Search);
  while Code = 0 do
    begin
      Count := Count + 1;
      if Search.Name = 'alpha1.dat' then
        begin
          if Search.Size <> 3 then
            Halt(13);
          FoundOne := True
        end
      else if Search.Name = 'alpha2.dat' then
        begin
          if Search.Size <> 5 then
            Halt(14);
          FoundTwo := True
        end
      else
        Halt(15);
      LastName := Search.Name;
      Code := FindNext(Search)
    end;
  if Code <> -1 then
    Halt(16);
  if Count <> 2 then
    Halt(17);
  if not FoundOne or not FoundTwo then
    Halt(18);
  if Search.Name <> LastName then
    Halt(19);
  if FindNext(Search) <> -1 then
    Halt(20);
  FindClose(Search);

  if FindFirst('files/*.none', faAnyFile, Search) <> -1 then
    Halt(21);
  if Search.FindHandle <> nil then
    Halt(22);
  if FindNext(Search) <> -1 then
    Halt(23);
  FindClose(Search);

  if FindFirst('', faAnyFile, Search) <> -1 then
    Halt(24);
  if Search.FindHandle <> nil then
    Halt(25);

  if FindFirst('missing', faAnyFile, Search) <> -1 then
    Halt(26);
  if Search.FindHandle <> nil then
    Halt(27);

  if FindFirst('files/subdir',
    faAnyFile and not faDirectory, Search) <> -1 then
    Halt(28);
  if Search.FindHandle <> nil then
    Halt(29);
  if FindFirst('files/subdir', faDirectory, Search) <> 0 then
    Halt(30);
  if (Search.Attr and faDirectory) = 0 then
    Halt(31);
  FindClose(Search);

  if FindFirst('files/readonly.dat', 0, Search) <> 0 then
    Halt(32);
  if (Search.Attr and faReadOnly) = 0 then
    Halt(33);
  FindClose(Search);

  if FindFirst('files/.*',
    (faAnyFile and not faHidden) and not faDirectory,
    Search) <> -1 then
    Halt(34);
  if FindFirst('files/.*',
    (faAnyFile or faHidden) and not faDirectory,
    Search) <> 0 then
    Halt(35);
  if Search.Name <> '.secret' then
    Halt(36);
  if (Search.Attr and faHidden) = 0 then
    Halt(37);
  FindClose(Search);

  if FindFirst('files/pipe',
    faAnyFile and not faSysFile, Search) <> -1 then
    Halt(38);
  if FindFirst('files/pipe', faSysFile, Search) <> 0 then
    Halt(39);
  if (Search.Attr and faSysFile) = 0 then
    Halt(40);
  FindClose(Search);

  if FindFirst('files/file-link',
    faAnyFile, Search) <> 0 then
    Halt(41);
  if (Search.Attr and faSymLink) <> 0 then
    Halt(42);
  if Search.Size <> 3 then
    Halt(43);
  FindClose(Search);

  if FindFirst('files/file-link',
    faSymLink, Search) <> 0 then
    Halt(44);
  if (Search.Attr and faSymLink) = 0 then
    Halt(45);
  FindClose(Search);

  if FindFirst('files/dir-link',
    faSymLink, Search) <> -1 then
    Halt(46);
  if FindFirst('files/dir-link',
    faSymLink or faDirectory, Search) <> 0 then
    Halt(47);
  if (Search.Attr and faSymLink) = 0 then
    Halt(48);
  if (Search.Attr and faDirectory) = 0 then
    Halt(49);
  FindClose(Search)
end.
