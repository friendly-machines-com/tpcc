program old_file_api;

type
  TBytes = array[0..3] of Byte;

procedure WriteBuffer(var Destination: File; const Buffer;
  Count: LongInt; var Written: LongInt);
begin
  BlockWrite(Destination, (@Buffer)^, Count, Written)
end;

var
  F: File;
  Position: Int64;
  Size: Int64;
  Status: Word;
  Written: LongInt;
  ReadCount: LongInt;
  Source: TBytes;
  Destination: TBytes;
begin
  WriteLn(FileMode);
  Assign(F, 'old_file_api.tmp');
  Rewrite(F, 1);
  WriteLn(IOResult);
  Seek(F, 4);
  WriteLn(IOResult);
  Position := FilePos(F);
  WriteLn(Position);
  Truncate(F);
  WriteLn(IOResult);
  Size := FileSize(F);
  WriteLn(Size);
  Close(F);
  WriteLn(IOResult);

  FileMode := 0;
  Reset(F, 1);
  WriteLn(FileSize(F));
  WriteLn(EOF(F));
  Seek(F, 4);
  WriteLn(EOF(F));
  Close(F);
  WriteLn(IOResult);

  Assign(F, 'missing-old-file-api.tmp');
  {$push}{$I-}
  Reset(F, 1);
  {$pop}
  Status := IOResult;
  if Status = 0 then
    WriteLn(0)
  else
    WriteLn(1);

  Source[0] := 10;
  Source[1] := 20;
  Source[2] := 30;
  Source[3] := 40;
  Assign(F, 'old_file_data.tmp');
  Rewrite(F, 1);
  WriteBuffer(F, Source, 4, Written);
  WriteLn(Written);
  Close(F);
  WriteLn(IOResult);
  FileMode := 0;
  Reset(F, 1);
  BlockRead(F, Destination, LongInt(4), ReadCount);
  WriteLn(ReadCount);
  WriteLn(Destination[0]);
  WriteLn(Destination[1]);
  WriteLn(Destination[2]);
  WriteLn(Destination[3]);
  Close(F);
  WriteLn(IOResult)
end.
