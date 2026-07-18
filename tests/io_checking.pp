program IoChecking;

uses
  SysUtils;

type
  TResetProc = procedure(var F: File; RecordSize: LongInt);

var
  F: File;
  ClosedText: Text;
  Buffer: array[0..7] of Byte;
  Count, Transferred: LongInt;
  Position, Size: Int64;
  AtEnd: Boolean;
  Caught: Boolean;
  ErrorCode: Integer;
  ResetProc: TResetProc;

procedure BeginCatch;
begin
  Caught := False;
  ErrorCode := -1
end;

procedure Remember(E: EInOutError);
begin
  Caught := True;
  ErrorCode := E.ErrorCode
end;

procedure CheckCaught(Expected, Failure: Integer);
begin
  if not Caught then
    Halt(Failure);
  if ErrorCode <> Expected then
    Halt(Failure + 1);
  // Checked I/O consumes the status before invoking ErrorProc.
  if IOResult <> 0 then
    Halt(Failure + 2)
end;

procedure CheckStatus(Expected, Failure: Integer);
begin
  if IOResult <> Expected then
    Halt(Failure)
end;

begin
  Count := 1;

  // I/O checking is on by default.
  BeginCatch;
  try
    Reset(F)
  except
    on E: EInOutError do
      Remember(E)
  end;
  CheckCaught(102, 1);

  // Every implemented old-style operation has an unchecked entry point.
  {$I-}
  Rewrite(F);
  CheckStatus(102, 4);
  Reset(F);
  CheckStatus(102, 5);
  Close(F);
  CheckStatus(102, 6);
  Seek(F, 0);
  CheckStatus(102, 7);
  Position := FilePos(F);
  CheckStatus(102, 8);
  Size := FileSize(F);
  CheckStatus(102, 9);
  AtEnd := Eof(F);
  CheckStatus(102, 10);
  Truncate(F);
  CheckStatus(102, 11);
  BlockRead(F, Buffer, Count, Transferred);
  CheckStatus(102, 12);
  BlockRead(F, Buffer, Count);
  CheckStatus(102, 13);
  BlockWrite(F, Buffer, Count, Transferred);
  CheckStatus(102, 14);
  BlockWrite(F, Buffer, Count);
  CheckStatus(102, 15);
  Write(ClosedText, 'x');
  CheckStatus(103, 16);
  WriteLn(ClosedText, 'x');
  CheckStatus(103, 17);

  // Every checked entry point raises instead of returning a sentinel.
  {$I+}
  BeginCatch;
  try
    Rewrite(F)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 20);

  BeginCatch;
  try
    Close(F)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 23);

  BeginCatch;
  try
    Seek(F, 0)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 26);

  BeginCatch;
  try
    Position := FilePos(F)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 29);

  BeginCatch;
  try
    Size := FileSize(F)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 32);

  BeginCatch;
  try
    AtEnd := Eof(F)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 35);

  BeginCatch;
  try
    Truncate(F)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 38);

  BeginCatch;
  try
    BlockRead(F, Buffer, Count, Transferred)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 41);

  BeginCatch;
  try
    BlockRead(F, Buffer, Count)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 44);

  BeginCatch;
  try
    BlockWrite(F, Buffer, Count, Transferred)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 47);

  BeginCatch;
  try
    BlockWrite(F, Buffer, Count)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 50);

  BeginCatch;
  try
    Write(ClosedText, 'x')
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(103, 53);

  BeginCatch;
  try
    WriteLn(ClosedText, 'x')
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(103, 56);

  // Entering checked mode with an old unchecked error raises and consumes
  // that original error before attempting the new operation.
  {$I-}
  Reset(F);
  BeginCatch;
  {$I+}
  try
    Position := FilePos(F)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 59);

  // A routine value uses the canonical checked Pascal-visible entry point;
  // caller {$I-} cannot dynamically change an indirect call's ABI.
  ResetProc := @Reset;
  BeginCatch;
  {$I-}
  try
    ResetProc(F, 128)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 62);

  // Invalid record size and seek-position failures must also be classified as
  // EInOutError rather than the generic runtime Exception.
  Assign(F, '/tmp/tpcc-i-check-runtime.tmp');
  BeginCatch;
  {$I+}
  try
    Rewrite(F, 0)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(12, 65);

  Rewrite(F);
  BeginCatch;
  try
    Seek(F, -1)
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(156, 68);

  // Assign is a status-gated association operation, not caller-$I I/O. An
  // already-open file is invalid and raises instead of hiding a Close.
  BeginCatch;
  {$I-}
  try
    Assign(F, '/tmp/tpcc-i-check-other.tmp')
  except
    on E: EInOutError do Remember(E)
  end;
  CheckCaught(102, 71);

  {$I+}
  Close(F)
end.
