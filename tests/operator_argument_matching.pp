program OperatorArgumentMatching;

type
  TBox = record
    Value: Integer;
  end;
  TSmall = 1..10;
  TStableMarker = record
    Value: Integer;
  end;

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
  StableMarker: TStableMarker;

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

function Literal16(Value: SmallInt): Integer; overload;
begin
  if Value = Value then
    Result := 21
end;

function Literal16(Value: Word): Integer; overload;
begin
  if Value = Value then
    Result := 22
end;

function Literal32(Value: Integer): Integer; overload;
begin
  if Value = Value then
    Result := 23
end;

function Literal32(Value: Cardinal): Integer; overload;
begin
  if Value = Value then
    Result := 24
end;

function LiteralCrossWidth(Value: Int64): Integer; overload;
begin
  if Value = Value then
    Result := 25
end;

function LiteralCrossWidth(Value: Byte): Integer; overload;
begin
  if Value = Value then
    Result := 26
end;

function LiteralNatural(Value: ShortInt): Integer; overload;
begin
  if Value = Value then Result := 30
end;

function LiteralNatural(Value: Byte): Integer; overload;
begin
  if Value = Value then Result := 31
end;

function LiteralNatural(Value: SmallInt): Integer; overload;
begin
  if Value = Value then Result := 32
end;

function LiteralNatural(Value: Word): Integer; overload;
begin
  if Value = Value then Result := 33
end;

function LiteralNatural(Value: Integer): Integer; overload;
begin
  if Value = Value then Result := 34
end;

function LiteralNatural(Value: Cardinal): Integer; overload;
begin
  if Value = Value then Result := 35
end;

function LiteralNatural(Value: Int64): Integer; overload;
begin
  if Value = Value then Result := 36
end;

function LiteralNatural(Value: QWord): Integer; overload;
begin
  if Value = Value then Result := 37
end;

function WideLiteral(Value: Int64): Integer; overload;
begin
  if Value = Value then Result := 40
end;

function WideLiteral(Value: QWord): Integer; overload;
begin
  if Value = Value then Result := 41
end;

function DistanceBeforeSign(Value: SmallInt): Integer; overload;
begin
  if Value = Value then Result := 42
end;

function DistanceBeforeSign(Value: Cardinal): Integer; overload;
begin
  if Value = Value then Result := 43
end;

function EqualDistanceSign(Value: SmallInt): Integer; overload;
begin
  if Value = Value then Result := 44
end;

function EqualDistanceSign(Value: Word): Integer; overload;
begin
  if Value = Value then Result := 45
end;

function MixedLiteral(Value, Count: Cardinal): Integer; overload;
begin
  if (Value = Value) and (Count = Count) then Result := 46
end;

function MixedLiteral(Value, Count: Int64): Integer; overload;
begin
  if (Value = Value) and (Count = Count) then Result := 47
end;

function StableExact(Value: Byte): Integer; overload;
begin
  if Value = Value then Result := 50
end;

function StableExact(Value: Int64): Integer; overload;
begin
  if Value = Value then Result := 51
end;

function StableExact(Value: Byte; Marker: TStableMarker): Integer; overload;
begin
  if (Value = Value) and (Marker.Value = Marker.Value) then Result := 50
end;

function StableExact(Value: Int64; Marker: TStableMarker): Integer; overload;
begin
  if (Value = Value) and (Marker.Value = Marker.Value) then Result := 51
end;

function StableWiden(Value: SmallInt): Integer; overload;
begin
  if Value = Value then Result := 52
end;

function StableWiden(Value: Int64): Integer; overload;
begin
  if Value = Value then Result := 53
end;

function StableWiden(Value: SmallInt; Marker: TStableMarker): Integer; overload;
begin
  if (Value = Value) and (Marker.Value = Marker.Value) then Result := 52
end;

function StableWiden(Value: Int64; Marker: TStableMarker): Integer; overload;
begin
  if (Value = Value) and (Marker.Value = Marker.Value) then Result := 53
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
  StableMarker.Value := 1;

  { Adding the same compatible formal/actual coordinate cannot rebalance the
    existing coordinates of an overload family. }
  if StableExact(LeftByte) <> 50 then Halt(50);
  if StableExact(LeftByte, StableMarker) <> 50 then Halt(51);
  if StableWiden(Signed8) <> 52 then Halt(52);
  if StableWiden(Signed8, StableMarker) <> 52 then Halt(53);

  if NumericKind(LeftByte + RightByte) <> 11 then
    Halt(3);
  if NumericKind(Signed32 + Unsigned32) <> 12 then
    Halt(4);
  { Neither Int64 -> QWord nor QWord -> Int64 is an implicit assignment
    edge. Extended is the first declared arithmetic formal accepting both. }
  if NumericKind(Signed64 + Unsigned64) <> 15 then
    Halt(5);
  if NumericKind(Real32 + Real32) <> 15 then
    Halt(6);
  if NumericKind(LeftByte / RightByte) <> 15 then
    Halt(7);
  if NumericKind(LeftByte and RightByte) <> 11 then
    Halt(8);
  if NumericKind(SmallConstant) <> 16 then
    Halt(9);
  if NumericKind(WideConstant) <> 12 then
    Halt(10);
  if NumericKind(SmallConstant + 1) <> 11 then
    Halt(11);
  if NumericKind(+SmallConstant) <> 16 then
    Halt(12);
  if NumericKind(-SmallConstant) <> 16 then
    Halt(13);
  if NumericKind(Unsigned64 + Signed32) <> 15 then
    Halt(16);
  if NumericKind(Unsigned64 + Signed64) <> 15 then
    Halt(17);
  if NumericKind(Unsigned16 + Signed8) <> 11 then
    Halt(18);
  if NumericKind(LeftByte + Signed8) <> 11 then
    Halt(19);
  if Literal16(42) <> 21 then
    Halt(20);
  if Literal32(42) <> 23 then
    Halt(21);
  { The literal value fits Byte directly, and Byte has a direct widening edge
    to Int64. The narrower viable formal therefore wins. }
  if LiteralCrossWidth(42) <> 26 then
    Halt(22);

  { FPC gives an integer literal the smallest predefined carrier containing
    its value. Positive literals therefore alternate signed and unsigned
    carriers at their range boundaries. }
  if LiteralNatural(-129) <> 32 then Halt(23);
  if LiteralNatural(-128) <> 30 then Halt(24);
  if LiteralNatural(127) <> 30 then Halt(25);
  if LiteralNatural(128) <> 31 then Halt(26);
  if LiteralNatural(255) <> 31 then Halt(27);
  if LiteralNatural(256) <> 32 then Halt(28);
  if LiteralNatural(32767) <> 32 then Halt(29);
  if LiteralNatural(32768) <> 33 then Halt(30);
  if LiteralNatural(65535) <> 33 then Halt(31);
  if LiteralNatural(65536) <> 34 then Halt(32);
  if LiteralNatural(2147483647) <> 34 then Halt(33);
  if LiteralNatural(2147483648) <> 35 then Halt(34);
  if LiteralNatural(4294967295) <> 35 then Halt(35);
  if LiteralNatural(4294967296) <> 36 then Halt(36);
  if LiteralNatural(9223372036854775808) <> 37 then Halt(37);

  { Signedness breaks a tie only when neither destination assigns directly to
    the other. }
  if WideLiteral(4) <> 40 then Halt(38);
  if WideLiteral(200) <> 41 then Halt(39);
  if DistanceBeforeSign(200) <> 43 then Halt(40);
  if EqualDistanceSign(200) <> 45 then Halt(41);

  { Cardinal assigns directly to Int64, not conversely, so it is the narrower
    common formal whenever both candidates are otherwise viable. }
  if MixedLiteral(LeftByte, 4) <> 46 then Halt(42);
  if MixedLiteral(LeftByte, 200) <> 46 then Halt(43);
  if MixedLiteral(LeftByte, 300) <> 46 then Halt(44);
  if MixedLiteral(LeftByte, 40000) <> 46 then Halt(45);
  SmallLeft := 2;
  if MixedLiteral(SmallLeft, 4) <> 46 then Halt(46);
  if MixedLiteral(SmallLeft, 200) <> 46 then Halt(47);
  if MixedLiteral(SmallLeft, 300) <> 46 then Halt(48);
  if MixedLiteral(SmallLeft, 40000) <> 46 then Halt(49);

  SmallRight := 3;
  Sum := SmallLeft + SmallRight;
  if Selected <> 3 then
    Halt(14);
  if Sum.Value <> 5 then
    Halt(15)
end.
