program BasedIntegerLiterals;

const
  SignedBytePattern: ShortInt = $FF;
  Signed64Pattern: Int64 = $AAAAAAAAAAAAAAAA;
  Untyped64Pattern = $EFEFEFEFEFEFEFEF;
  NamedSigned64Pattern: Int64 = Untyped64Pattern;
  CombinedMask = $1000 or $8000;
  IntersectedMask = $FFFF and $8000;
  ToggledMask = $1000 xor $8000;

var
  Signed64: Int64;
  SignedPart: SmallInt;
  UnsignedPart: Word;
  MixedMask: Integer;
  BooleanAnd: Boolean;
  BooleanOr: Boolean;
  BooleanXor: Boolean;

function LiteralKind(Value: Int64): Integer; overload;
begin
  if Value <> Value then
    Halt(1);
  Result := 1
end;

function LiteralKind(Value: QWord): Integer; overload;
begin
  if Value <> Value then
    Halt(2);
  Result := 2
end;

function TakeSigned64(Value: Int64): Int64;
begin
  Result := Value
end;

begin
  if SignedBytePattern <> -1 then
    Halt(3);
  if Signed64Pattern <> -6148914691236517206 then
    Halt(4);
  if NamedSigned64Pattern <> -1157442765409226769 then
    Halt(5);

  Signed64 := $AAAAAAAAAAAAAAAA;
  if Signed64 <> Signed64Pattern then
    Halt(6);

  { The natural QWord overload remains better than the contextual Int64
    bit-pattern construction. }
  if LiteralKind($AAAAAAAAAAAAAAAA) <> 2 then
    Halt(7);

  { A singleton value formal accepts exactly what an Int64 assignment does. }
  if TakeSigned64($AAAAAAAAAAAAAAAA) <> Signed64Pattern then
    Halt(8);

  if CombinedMask <> $9000 then
    Halt(9);
  if IntersectedMask <> $8000 then
    Halt(10);
  if ToggledMask <> $9000 then
    Halt(11);

  SignedPart := $1000;
  UnsignedPart := $8000;
  MixedMask := SignedPart or UnsignedPart;
  if MixedMask <> $9000 then
    Halt(12);

  BooleanAnd := True and False;
  BooleanOr := True or False;
  BooleanXor := True xor True;
  if BooleanAnd or (not BooleanOr) or BooleanXor then
    Halt(13)
end.
