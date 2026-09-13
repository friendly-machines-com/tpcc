program RecentRTLExits;
var OldExit: Pointer;
procedure First;
begin
  WriteLn('first');
  ExitProc := OldExit
end;
procedure Last;
begin
  WriteLn('last')
end;
procedure ObserveCode;
begin
  WriteLn(ExitCode)
end;
procedure ChangeCode;
begin
  ExitCode := 23
end;
begin
  case ParamStr(1) of
    'normal': ExitCode := 7;
    'halt': Halt(9);
    'chain':
      begin
        ExitProc := @Last; OldExit := ExitProc;
        ExitProc := @First
      end;
    'halt-chain':
      begin
        ExitProc := @Last; OldExit := ExitProc;
        ExitProc := @First; Halt(9)
      end;
    'observe': begin ExitProc := @ObserveCode; Halt(9) end;
    'change': begin ExitProc := @ChangeCode; Halt(9) end;
    'assert-ok': Assert(True);
    'assert-fail': Assert(False);
    'assert-message': Assert(False, 'assert marker');
    'error': RunError(201);
    'output': WriteLn(Output, 'output ok')
  else Halt(99)
  end
end.
