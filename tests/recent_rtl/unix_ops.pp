program RecentRTLUnix;
uses BaseUnix, Unix;
var
  T: Text;
  F: File;
  Old, Previous: SignalHandler;
  Received: LongInt;
  Count: Word;
  C: Char;
  Line: AnsiString;
function RaiseSignal(Sig: LongInt): LongInt; external name '::raise';
procedure Handler(Sig: LongInt);
begin
  Received := Sig
end;
procedure OtherHandler(Sig: LongInt);
begin
  Received := -Sig
end;
begin
  case ParamStr(1) of
    'chmod':
      begin
        Assign(T, 'mode.txt'); Rewrite(T); Close(T);
        if FpChmod('mode.txt', 384) <> 0 then Halt(1);
        if FpChmod('does-not-exist', 384) <> -1 then Halt(2)
      end;
    'signal':
      begin
        Old := FpSignal(SIGINT, @Handler);
        if RaiseSignal(SIGINT) <> 0 then Halt(3);
        if Received <> SIGINT then Halt(4);
        Previous := FpSignal(SIGINT, @OtherHandler);
        if RaiseSignal(SIGINT) <> 0 then Halt(17);
        if Received <> -SIGINT then Halt(18);
        Previous := FpSignal(SIGINT, Previous);
        if RaiseSignal(SIGINT) <> 0 then Halt(19);
        if Received <> SIGINT then Halt(20);
        Old := FpSignal(SIGINT, Old)
      end;
    'text-write':
      begin
        if POpen(T, 'cat > pipe.txt', 'W') <> 0 then Halt(5);
        WriteLn(T, 'pipe data');
        if PClose(T) <> 0 then Halt(6)
      end;
    'text-read':
      begin
        if POpen(T, 'printf X', 'R') <> 0 then Halt(7);
        ReadLn(T, Line);
        if Line <> 'X' then Halt(8);
        if PClose(T) <> 0 then Halt(9)
      end;
    'file-read':
      begin
        if POpen(F, 'printf X', 'R') <> 0 then Halt(10);
        BlockRead(F, C, 1, Count);
        if (Count <> 1) or (C <> 'X') then Halt(11);
        if PClose(F) <> 0 then Halt(12)
      end;
    'text-status':
      begin
        if POpen(T, 'exit 7', 'R') <> 0 then Halt(13);
        if PClose(T) <> 7 then Halt(14)
      end;
    'file-status':
      begin
        if POpen(F, 'exit 7', 'R') <> 0 then Halt(15);
        if PClose(F) <> 7 then Halt(16)
      end
  else Halt(99)
  end
end.
