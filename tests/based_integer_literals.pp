program BasedIntegerLiterals;

const
  SignedBytePattern: ShortInt = $FF;
  Signed64Pattern: Int64 = $AAAAAAAAAAAAAAAA;
  Untyped64Pattern = $EFEFEFEFEFEFEFEF;
  NamedSigned64Pattern: Int64 = Untyped64Pattern;

var
  Signed64: Int64;

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
    Halt(8)
end.
