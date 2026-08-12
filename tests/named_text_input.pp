program NamedTextInput;

var
  F: Text;
  FileName: ShortString;
  Buffer: array[0..63] of Byte;
  ShortLine: ShortString;
  LongLine: AnsiString;

begin
  FileName := ParamStr(1);
  Assign(F, FileName);
  Reset(F);
  SetTextBuf(F, Buffer, SizeOf(Buffer));

  ReadLn(F, ShortLine);
  if ShortLine <> 'alpha' then
    Halt(1);

  ReadLn(F, LongLine);
  if Length(LongLine) <> 300 then
    Halt(2);
  if (LongLine[1] <> 'x') or
     (LongLine[300] <> 'x') then
    Halt(3);

  ReadLn(F, ShortLine);
  if ShortLine <> 'omega' then
    Halt(4);
  if not EOF(F) then
    Halt(5);

  Close(F);
  Finalize(F)
end.
