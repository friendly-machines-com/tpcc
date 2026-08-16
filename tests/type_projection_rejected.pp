program TypeProjectionRejected;

type
  TArray = array[0..1] of Integer;
  TRecord = record
    Value: Integer;
  end;

var
  RuntimeIndex: Integer;

type
  {$ifdef REJECT_NON_POINTER}
  TRejected = Integer^;
  {$endif}
  {$ifdef REJECT_UNTYPED_POINTER}
  TRejected = Pointer^;
  {$endif}
  {$ifdef REJECT_NON_ARRAY}
  TRejected = Integer[0];
  {$endif}
  {$ifdef REJECT_NON_CONSTANT_INDEX}
  TRejected = TArray[RuntimeIndex];
  {$endif}
  {$ifdef REJECT_WRONG_INDEX_TYPE}
  TRejected = TArray[False];
  {$endif}
  {$ifdef REJECT_MISSING_FIELD}
  TRejected = TRecord.Missing;
  {$endif}

begin
end.
