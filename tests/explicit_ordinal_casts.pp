program ExplicitOrdinalCasts;

type
  TSmallEnum = (Zero, One);

const
  FoldedByte = Byte(300);
  FoldedSigned = ShortInt(255);
  FoldedEnumSource = Byte(One);
  FoldedCharSource = Byte('A');
  FoldedEnumTarget = TSmallEnum(257);
  FoldedBoolean = Boolean(257);

var
  B: Byte;
  S: ShortInt;
  I: Integer;
  E: TSmallEnum;

begin
  B := Byte(300);
  if B <> 44 then
    Halt(1);
  S := ShortInt(255);
  if S <> -1 then
    Halt(2);
  I := Integer(Cardinal($FFFFFFFF));
  if I <> -1 then
    Halt(3);
  if FoldedByte <> 44 then
    Halt(4);
  if FoldedSigned <> -1 then
    Halt(5);
  if FoldedEnumSource <> 1 then
    Halt(6);
  if FoldedCharSource <> 65 then
    Halt(7);
  E := TSmallEnum(257);
  if Ord(E) <> 257 then
    Halt(8);
  if Ord(FoldedEnumTarget) <> 257 then
    Halt(9);
  if Ord(FoldedBoolean) <> 1 then
    Halt(10);
  if Ord(Boolean(256)) <> 0 then
    Halt(11)
end.
