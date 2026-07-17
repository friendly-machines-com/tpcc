program ImplicitConversionSingleEdge;

type
  TA = record
    Value: Integer;
  end;
  TB = record
    Value: Integer;
  end;
  TC = record
    Value: Integer;
  end;
  TResult = record
    Value: Integer;
  end;
  TLiteralResult = record
    Value: Integer;
  end;
  TText = string[8];
  TByteSet = set of Byte;
  PInteger = ^Integer;
  TTextResult = record
    Value: Integer;
  end;
  TSetResult = record
    Value: Integer;
  end;
  TNilResult = record
    Value: Integer;
  end;
  TNarrowResult = record
    Value: Integer;
  end;
  TSmallRange = 0..10;
  TSubrangeResult = record
    Value: Integer;
  end;

const
  Five = 5;

var
  A: TA;
  C: TC;
  Small: ShortInt;
  Converted: TResult;
  LiteralConverted: TLiteralResult;
  ByteSet: TByteSet;
  TextConverted: TTextResult;
  SetConverted: TSetResult;
  NilConverted: TNilResult;
  Wide: Integer;
  NarrowConverted: TNarrowResult;
  SmallRange: TSmallRange;
  SubrangeConverted: TSubrangeResult;

operator implicit(const Value: TA): TB;
begin
  Result.Value := Value.Value + 10
end;

operator implicit(const Value: TB): TC;
begin
  Result.Value := Value.Value + 20
end;

operator implicit(const Value: Integer): TResult;
begin
  Result.Value := Value + 30
end;

operator implicit(const Value: Integer): TLiteralResult;
begin
  Result.Value := Value + 30
end;

operator implicit(const Value: TText): TTextResult;
begin
  Result.Value := Length(Value)
end;

operator implicit(const Value: TByteSet): TSetResult;
begin
  if 2 in Value then
    Result.Value := 2
  else
    Result.Value := 0
end;

operator implicit(Value: PInteger): TNilResult;
begin
  Value := Value;
  Result.Value := 1
end;

operator implicit(const Value: ShortInt): TNarrowResult;
begin
  Result.Value := Integer(Value) + 40
end;

operator implicit(const Value: Integer): TSubrangeResult;
begin
  Result.Value := Value + 50
end;

{$ifndef OMIT_RECORD_DIRECT}
operator implicit(const Value: TA): TC;
begin
  Result.Value := Value.Value + 100
end;
{$endif}

{$ifndef OMIT_INTEGER_DIRECT}
operator implicit(const Value: ShortInt): TResult;
begin
  Result.Value := Integer(Value) + 200
end;
{$endif}

{$ifndef OMIT_NARROW_DIRECT}
operator implicit(const Value: Integer): TNarrowResult;
begin
  Result.Value := Value + 400
end;
{$endif}

{$ifndef OMIT_SUBRANGE_DIRECT}
operator implicit(const Value: TSmallRange): TSubrangeResult;
begin
  Result.Value := Integer(Value) + 500
end;
{$endif}

begin
  A.Value := 1;
  C := A;
  if C.Value <> 101 then
    Halt(1);
  Small := 2;
  Converted := Small;
  if Converted.Value <> 202 then
    Halt(2);
  LiteralConverted := 5;
  if LiteralConverted.Value <> 35 then
    Halt(3);
  LiteralConverted := Five;
  if LiteralConverted.Value <> 35 then
    Halt(4);
  TextConverted := 'abc';
  if TextConverted.Value <> 3 then
    Halt(5);
  ByteSet := [1, 2];
  SetConverted := ByteSet;
  if SetConverted.Value <> 2 then
    Halt(6);
  SetConverted := [1, 2];
  if SetConverted.Value <> 2 then
    Halt(7);
  NilConverted := nil;
  if NilConverted.Value <> 1 then
    Halt(8);
  Wide := 3;
  NarrowConverted := Wide;
  if NarrowConverted.Value <> 403 then
    Halt(9);
  SmallRange := 4;
  SubrangeConverted := SmallRange;
  if SubrangeConverted.Value <> 504 then
    Halt(10)
end.
