program RecentRTLPaths;
uses SysUtils;
var S: AnsiString; P: PChar;
begin
  case ParamStr(1) of
    'extension':
      begin
        if ExtractFileExt('/a.b/c.txt') <> '.txt' then Halt(1);
        if ExtractFileExt('/a.b/c') <> '' then Halt(2);
        if ExtractFileExt('a.') <> '.' then Halt(3)
      end;
    'scan':
      begin
        S := 'abca'; P := PChar(S);
        if StrRScan(P, 'a') <> P + 3 then Halt(4);
        if StrRScan(P, 'z') <> nil then Halt(5)
      end;
    'scan-nul':
      begin
        S := 'abca'; P := PChar(S);
        if StrRScan(P, #0) <> P + 4 then Halt(6)
      end;
    'separators':
      if SetDirSeparators('a\b/c') <> 'a/b/c' then Halt(7);
    'compare':
      begin
        if AnsiCompareFileName('abc', 'abc') <> 0 then Halt(8);
        if AnsiCompareFileName('abc', 'abd') >= 0 then Halt(9);
        if AnsiCompareFileName('ab', 'abc') >= 0 then Halt(10)
      end
  else Halt(99)
  end
end.
