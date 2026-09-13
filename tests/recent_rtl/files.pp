program RecentRTLFiles;
var T: Text; F: File; Code: Integer;
begin
  case ParamStr(1) of
    'append':
      begin
        Assign(T, 'append.txt'); Rewrite(T); WriteLn(T, 'first'); Close(T);
        Append(T); WriteLn(T, 'second'); Close(T)
      end;
    'append-missing':
      begin
        Assign(T, 'missing.txt');
        {$I-} Append(T); Code := IOResult; {$I+}
        if Code = 0 then begin Close(T); Halt(1) end
      end;
    'erase':
      begin
        Assign(T, 'text.txt'); Rewrite(T); Close(T); Erase(T);
        Assign(F, 'binary.dat'); Rewrite(F, 1); Close(F); Erase(F)
      end;
    'erase-open':
      begin
        Assign(T, 'open.txt'); Rewrite(T);
        {$I-} Erase(T); Code := IOResult; {$I+}
        if Code = 0 then Halt(2);
        Close(T)
      end;
    'mkdir': MkDir('directory');
    'mkdir-error':
      begin
        MkDir('directory');
        {$I-} MkDir('directory'); Code := IOResult; {$I+}
        if Code = 0 then Halt(3)
      end
  else Halt(99)
  end
end.
