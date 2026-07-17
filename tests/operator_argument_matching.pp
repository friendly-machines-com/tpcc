program OperatorArgumentMatching;

type
  TBox = record
    Value: Integer;
  end;
  TSmall = 1..10;

const
  SmallConstant = 3;
  WideConstant = 4294967296;

var
  Selected: Integer;
  Box: TBox;
  Sum: TBox;
  LeftByte, RightByte: Byte;
  Unsigned16: Word;
  Signed8: ShortInt;
  Signed32: Integer;
  Unsigned32: Cardinal;
  Signed64: Int64;
  Unsigned64: QWord;
  Real32: Single;
  SmallLeft, SmallRight: TSmall;

operator :=(Value: Integer): TBox;
begin
  Result.Value := Value
end;

operator +(Left, Right: TBox): TBox;
begin
  Selected := 1;
  Result.Value := Left.Value + Right.Value
end;

operator +(Left: TBox; Right: Integer): TBox;
begin
  Selected := 2;
  Result.Value := Left.Value + Right
end;

operator +(Left, Right: TSmall): TBox;
begin
  Selected := 3;
  if (Left = Left) and (Right = Right) then
    Result.Value := 5
end;

function NumericKind(Value: Byte): Integer; overload;
begin
  if Value = Value then
    Result := 10
end;

function NumericKind(Value: ShortInt): Integer; overload;
begin
  if Value = Value then
    Result := 16
end;

function NumericKind(Value: Integer): Integer; overload;
begin
  if Value = Value then
    Result := 11
end;

function NumericKind(Value: Int64): Integer; overload;
begin
  if Value = Value then
    Result := 12
end;

function NumericKind(Value: QWord): Integer; overload;
begin
  if Value = Value then
    Result := 13
end;

function NumericKind(Value: Single): Integer; overload;
begin
  if Value = Value then
    Result := 14
end;

function NumericKind(Value: Extended): Integer; overload;
begin
  if Value = Value then
    Result := 15
end;

begin
  Box.Value := 40;
  Sum := Box + 2;
  if Selected <> 2 then
    Halt(1);
  if Sum.Value <> 42 then
    Halt(2);

  LeftByte := 6;
  RightByte := 3;
  Unsigned16 := 2;
  Signed8 := -1;
  Signed32 := -1;
  Unsigned32 := 2;
  Signed64 := -1;
  Unsigned64 := 2;
  Real32 := 1.5;

  if NumericKind(LeftByte + RightByte) <> 11 then
    Halt(3);
  if NumericKind(Signed32 + Unsigned32) <> 12 then
    Halt(4);
  { Int64 is Pascal's preferred arithmetic promotion from QWord even though
    that conversion still requires a caller-side range check. A real-domain
    fallback must not steal an expression whose source operands are integers. }
  if NumericKind(Signed64 + Unsigned64) <> 12 then
    Halt(5);
  if NumericKind(Real32 + Real32) <> 15 then
    Halt(6);
  if NumericKind(LeftByte / RightByte) <> 15 then
    Halt(7);
  if NumericKind(LeftByte and RightByte) <> 11 then
    Halt(8);
  if NumericKind(SmallConstant) <> 10 then
    Halt(9);
  if NumericKind(WideConstant) <> 13 then
    Halt(10);
  if NumericKind(SmallConstant + 1) <> 11 then
    Halt(11);
  if NumericKind(+SmallConstant) <> 10 then
    Halt(12);
  if NumericKind(-SmallConstant) <> 16 then
    Halt(13);
  if NumericKind(Unsigned64 + Signed32) <> 13 then
    Halt(16);
  if NumericKind(Unsigned64 + Signed64) <> 12 then
    Halt(17);
  if NumericKind(Unsigned16 + Signed8) <> 11 then
    Halt(18);
  if NumericKind(LeftByte + Signed8) <> 11 then
    Halt(19);

  SmallLeft := 2;
  SmallRight := 3;
  Sum := SmallLeft + SmallRight;
  if Selected <> 3 then
    Halt(14);
  if Sum.Value <> 5 then
    Halt(15)
end.
