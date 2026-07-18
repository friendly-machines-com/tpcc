program PredefinedExplicitRejected;

type
  TRecordA = record
    Value: Integer;
  end;
  TRecordB = record
    Value: Integer;
  end;
  TArrayA = array[1..2] of Integer;
  TArrayB = array[1..2] of Integer;
  TBase = class
  end;
  TUnrelated = class
  end;
  TPackedAnsi = packed record
    Bytes: array[1..8] of Byte;
  end;
  TByteSet = set of Byte;
  TPackedSet = packed record
    Bytes: array[1..24] of Byte;
  end;
  TChainA = record
    Value: Integer;
  end;
  TChainB = record
    Value: Integer;
  end;
  TChainC = record
    Value: Integer;
  end;

operator Implicit(const Value: TChainA): TChainB;
begin
  Result.Value := Value.Value
end;

operator Explicit(const Value: TChainB): TChainC;
begin
  Result.Value := Value.Value
end;

var
  RecordA: TRecordA;
  RecordB: TRecordB;
  ArrayA: TArrayA;
  ArrayB: TArrayB;
  RealValue: Double;
  IntegerValue: Integer;
  Base: TBase;
  Unrelated: TUnrelated;
  TextValue: AnsiString;
  PackedAnsi: TPackedAnsi;
  Values: TByteSet;
  PackedSet: TPackedSet;
  ChainA: TChainA;
  ChainC: TChainC;

begin
  {$ifdef REJECT_RECORD}
  RecordB := TRecordB(RecordA);
  {$endif}
  {$ifdef REJECT_ARRAY}
  ArrayB := TArrayB(ArrayA);
  {$endif}
  {$ifdef REJECT_REAL_ORDINAL}
  IntegerValue := Integer(RealValue);
  {$endif}
  {$ifdef REJECT_UNRELATED_CLASS}
  Unrelated := TUnrelated(Base);
  {$endif}
  {$ifdef REJECT_MANAGED_PACKED}
  PackedAnsi := TPackedAnsi(TextValue);
  {$endif}
  {$ifdef REJECT_SET_PACKED}
  PackedSet := TPackedSet(Values);
  {$endif}
  {$ifdef REJECT_CHAIN}
  ChainC := TChainC(ChainA);
  {$endif}
end.
