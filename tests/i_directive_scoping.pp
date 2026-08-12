program IDirectiveScoping;

type
  TResetProc = procedure(var F: File; RecordSize: LongInt);

var
{$ifopt I+}
  DefaultIIsOn: Integer;
{$else}
  WrongDefaultI: MissingType;
{$endif}
  DefaultResetFile: File;
  CheckedResetFile, UncheckedResetFile: File;
  CheckedRewriteFile, UncheckedRewriteFile: File;
  AnchoredCheckedFile, AnchoredUncheckedFile: File;
  CheckedCloseFile, UncheckedCloseFile: File;
  CheckedSeekFile, UncheckedSeekFile: File;
  CheckedPositionFile, UncheckedPositionFile: File;
  CheckedSizeFile, UncheckedSizeFile: File;
  CheckedEofFile, UncheckedEofFile: File;
  CheckedTruncateFile, UncheckedTruncateFile: File;
  CheckedReadFile, UncheckedReadFile: File;
  CheckedWriteFile, UncheckedWriteFile: File;
  AliasCheckedFile, AliasUncheckedFile: File;
  PointerFile: File;
  CheckedFlushText, UncheckedFlushText: Text;
  CheckedBuffer, UncheckedBuffer: array[0..7] of Byte;
  CheckedCount, UncheckedCount: LongInt;
  CheckedPosition, UncheckedPosition: Int64;
  CheckedSize, UncheckedSize: Int64;
  CheckedAtEnd, UncheckedAtEnd: Boolean;
  CheckedRewriteText, UncheckedRewriteText: Text;
  Marker: Integer;
  ResetProc: TResetProc;

procedure Reset(var Value: Integer);
begin
  Value := Value + 1
end;

begin
  System.Reset(DefaultResetFile);

  {$I+}
  System.Reset(CheckedResetFile);
  System.Rewrite(CheckedRewriteFile);
  System.Rewrite(CheckedRewriteText);
  System.Close(CheckedCloseFile);
  System.Seek(CheckedSeekFile, 0);
  CheckedPosition := System.FilePos(CheckedPositionFile);
  CheckedSize := System.FileSize(CheckedSizeFile);
  CheckedAtEnd := System.Eof(CheckedEofFile);
  System.Truncate(CheckedTruncateFile);
  System.BlockRead(CheckedReadFile, CheckedBuffer, 1, CheckedCount);
  System.BlockWrite(CheckedWriteFile, CheckedBuffer, 1, CheckedCount);
  Write('checked-write');
  WriteLn('checked-writeln');
  System.Flush(CheckedFlushText);

  {$I-}
  System.Reset(UncheckedResetFile);
  System.Rewrite(UncheckedRewriteFile);
  System.Rewrite(UncheckedRewriteText);
  System.Close(UncheckedCloseFile);
  System.Seek(UncheckedSeekFile, 0);
  UncheckedPosition := System.FilePos(UncheckedPositionFile);
  UncheckedSize := System.FileSize(UncheckedSizeFile);
  UncheckedAtEnd := System.Eof(UncheckedEofFile);
  System.Truncate(UncheckedTruncateFile);
  System.BlockRead(UncheckedReadFile, UncheckedBuffer, 1, UncheckedCount);
  System.BlockWrite(UncheckedWriteFile, UncheckedBuffer, 1, UncheckedCount);
  Write('unchecked-write');
  WriteLn('unchecked-writeln');
  System.Flush(UncheckedFlushText);

  // Argument-subtree directives persist afterward, but cannot retroactively
  // change the Reset operation whose identifier has already been consumed.
  {$I+}
  System.Reset({$I-} AnchoredCheckedFile);
  {$I-}
  System.Reset({$I+} AnchoredUncheckedFile);

  {$IOCHECKS OFF}
  System.Reset(AliasUncheckedFile);
  {$IOCHECKS ON}
  System.Reset(AliasCheckedFile);

  // A user routine with the same Pascal spelling remains an ordinary call
  // in both directive states.
  {$I-}
  Reset(Marker);
  {$I+}
  Reset(Marker);

  // Routine values retain the canonical, checked Pascal-visible entry point.
  ResetProc := @System.Reset;
  {$I-}
  ResetProc(PointerFile, 128)
end.
