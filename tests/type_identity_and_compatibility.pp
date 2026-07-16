program TypeIdentityAndCompatibility;

type
  TPointerA = ^Integer;
  TPointerB = ^Integer;
  TPointerAlias = TPointerA;

  TStringA = string[12];
  TStringB = string[12];

  TSetA = set of Byte;
  TSetB = set of Byte;
  TNarrowSet = set of 1..10;
  TWideSet = set of 1..20;

  TRangeA = 1..5;
  TRangeB = 1..5;

  TArrayA = array[0..1] of Integer;
  TArrayB = array[0..1] of Integer;

  TRoutineA = procedure(Value: Integer);
  TRoutineB = procedure(Value: Integer);

  TObjectClass = class
  end;
  TClassRefA = class of TObjectClass;
  TClassRefB = class of TObjectClass;

  TRecordA = record
    Value: Integer;
  end;
  TRecordB = record
    Value: Integer;
  end;
  TRecordAlias = TRecordA;

  TEnumA = (EnumA0, EnumA1);
  TEnumB = (EnumB0, EnumB1);

  TConversionSource = record
    Value: Integer;
  end;
  TConversionResultA = record
    Value: Integer;
  end;
  TConversionResultB = record
    Value: Integer;
  end;

var
  PointerA: TPointerA;
  PointerB: TPointerB;
  PointerAlias: TPointerAlias;
  StringA: TStringA;
  StringB: TStringB;
  SetA: TSetA;
  SetB: TSetB;
  NarrowSet: TNarrowSet;
  WideSet: TWideSet;
  RangeA: TRangeA;
  RangeB: TRangeB;
  ArrayA: TArrayA;
  ArrayB: TArrayB;
  RoutineA: TRoutineA;
  RoutineB: TRoutineB;
  ClassRefA: TClassRefA;
  ClassRefB: TClassRefB;
  RecordA: TRecordA;
  RecordB: TRecordB;
  RecordAlias: TRecordAlias;
  EnumA: TEnumA;
  EnumB: TEnumB;
  ConversionSource: TConversionSource;
  ConversionResultA: TConversionResultA;
  ConversionResultB: TConversionResultB;

operator :=(const Source: TConversionSource): TConversionResultA; forward;
operator implicit(const Source: TConversionSource): TConversionResultB; forward;

operator implicit(const Source: TConversionSource): TConversionResultA;
begin
  Result.Value := Source.Value + 10
end;

operator :=(const Source: TConversionSource): TConversionResultB;
begin
  Result.Value := Source.Value + 20
end;

procedure Sink(Value: Integer);
begin
  if Value = -1 then
    Halt(20)
end;

procedure AcceptPointerAlias(var Value: TPointerAlias);
begin
  Value := Value
end;

function PickRecord(Value: TRecordA): Integer; overload;
begin
  Result := Value.Value + 10
end;

function PickRecord(Value: TRecordB): Integer; overload;
begin
  Result := Value.Value + 20
end;

function PickEnum(Value: TEnumA): Integer; overload;
begin
  if Ord(Value) = Cardinal(1) then
    Result := 31
  else
    Result := 30
end;

function PickEnum(Value: TEnumB): Integer; overload;
begin
  if Ord(Value) = Cardinal(1) then
    Result := 41
  else
    Result := 40
end;

begin
  PointerA := PointerB;
  PointerAlias := PointerA;
  AcceptPointerAlias(PointerA);

  StringB := 'hello';
  StringA := StringB;

  SetB := [1, 2];
  SetA := SetB;

  RangeB := 3;
  RangeA := RangeB;

  ArrayB[0] := 4;
  ArrayB[1] := 5;
  ArrayA[0] := ArrayB[0];
  ArrayA[1] := ArrayB[1];

  NarrowSet := [2, 7];
  WideSet := NarrowSet;

  RoutineB := @Sink;
  RoutineA := RoutineB;

  ClassRefA := ClassRefB;

  RecordA.Value := 1;
  RecordB.Value := 2;
  RecordAlias.Value := 3;
  EnumA := EnumA1;
  EnumB := EnumB1;

  if PickRecord(RecordA) <> 11 then
    Halt(1);
  if PickRecord(RecordB) <> 22 then
    Halt(2);
  if PickRecord(RecordAlias) <> 13 then
    Halt(3);
  if PickEnum(EnumA) <> 31 then
    Halt(4);
  if PickEnum(EnumB) <> 41 then
    Halt(5);
  if StringA <> 'hello' then
    Halt(6);
  if not (2 in SetA) then
    Halt(7);
  if RangeA <> 3 then
    Halt(8);
  if ArrayA[0] + ArrayA[1] <> 9 then
    Halt(9);
  if not Assigned(RoutineA) then
    Halt(10);
  if not (7 in WideSet) then
    Halt(11);

  ConversionSource.Value := 1;
  ConversionResultA := ConversionSource;
  ConversionResultB := ConversionSource;
  if ConversionResultA.Value <> 11 then
    Halt(12);
  if ConversionResultB.Value <> 21 then
    Halt(13)
end.
