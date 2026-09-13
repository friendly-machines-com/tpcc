program SysutilsFileOps;
uses SysUtils;
var
  F, Empty: File;
  Handle: LongInt;
  B: Byte;
begin
  { GetFileHandle reports -1 for a file variable that was never assigned.
    FPC returns 0 here; -1 is a deliberate choice, see rtl.h. }
  if GetFileHandle(Empty) <> -1 then Halt(1);

  Assign(F, 'ops.bin');
  Rewrite(F, 1);
  B := 65;
  BlockWrite(F, B, 1);
  Handle := GetFileHandle(F);
  if Handle < 0 then Halt(2);

  { FileGetDate agrees with FileAge, which also reports the modification time. }
  if FileGetDate(Handle) <> FileAge('ops.bin') then Halt(3);
  if FileGetDate(-1) <> -1 then Halt(4);

  { The handle form updates the modification time and observes it back.
    FPC 3.2.2's Linux handle form is a stub returning -1; tpcc implements it,
    matching tp2cc and letting the compiler's date copy actually work. }
  if FileSetDate(Handle, 1111111111) <> 0 then Halt(5);
  if FileGetDate(Handle) <> 1111111111 then Halt(6);
  Close(F);

  { GetFileHandle reports -1 once the file is closed. }
  if GetFileHandle(F) <> -1 then Halt(7);

  { The name form updates the same timestamp, and FileAge sees it. }
  if FileSetDate('ops.bin', 1000000000) <> 0 then Halt(8);
  if FileAge('ops.bin') <> 1000000000 then Halt(9);

  { RenameFile moves the file and reports missing sources as failure. }
  if not RenameFile('ops.bin', 'moved.bin') then Halt(10);
  if FileExists('ops.bin') then Halt(11);
  if not FileExists('moved.bin') then Halt(12);
  if RenameFile('missing.bin', 'other.bin') then Halt(13);
  if FileSetDate('missing.bin', 1000000000) <> -1 then Halt(14);

  WriteLn('sysutils file ops ok')
end.
