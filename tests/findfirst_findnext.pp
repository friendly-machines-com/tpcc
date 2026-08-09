program FindFirstFindNext;

uses
  SysUtils;

var
  Search: TRawByteSearchRec;
  Code, Count: LongInt;
  FoundOne, FoundTwo: Boolean;
  LastName: AnsiString;

begin
  if IncludeTrailingPathDelimiter('') <> '/' then
    Halt(50);
  if IncludeTrailingPathDelimiter('files') <> 'files/' then
    Halt(51);
  if IncludeTrailingPathDelimiter('files/') <> 'files/' then
    Halt(52);
  if IncludeTrailingPathDelimiter('/') <> '/' then
    Halt(53);
  if IncludeTrailingPathDelimiter('files\') <> 'files\/' then
    Halt(54);

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
