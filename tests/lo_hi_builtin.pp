program LoHiBuiltin;

var
  B: Byte;
  SC: ShortInt;
  W: Word;
  SI: SmallInt;
  I: Integer;
  L: LongInt;
  DW: DWord;
  V64: Int64;
  U64: QWord;

begin
  B := $EF;
  if Lo(B) <> Byte($0F) then
    Halt(1);
  if Hi(B) <> Byte($0E) then
    Halt(2);

  SC := $7F;
  if Lo(SC) <> Byte($7F) then
    Halt(3);
  if Hi(SC) <> Byte($00) then
    Halt(4);

  W := $1234;
  if Lo(W) <> Byte($34) then
    Halt(5);
  if Hi(W) <> Byte($12) then
    Halt(6);

  SI := SmallInt($80FF);
  if Lo(SI) <> Byte($FF) then
    Halt(7);
  if Hi(SI) <> Byte($80) then
    Halt(8);

  // Integer and LongInt are the same 32-bit carrier: both spellings select
  // the carrier's Word result.
  I := $12345678;
  if Lo(I) <> Word($5678) then
    Halt(9);
  if Hi(I) <> Word($1234) then
    Halt(10);

  L := LongInt($89ABCDEF);
  if Lo(L) <> Word($CDEF) then
    Halt(11);
  if Hi(L) <> Word($89AB) then
    Halt(12);

  DW := $DEADBEEF;
  if Lo(DW) <> Word($BEEF) then
    Halt(13);
  if Hi(DW) <> Word($DEAD) then
    Halt(14);

  V64 := $123456789ABCDEF0;
  if Lo(V64) <> DWord($9ABCDEF0) then
    Halt(15);
  if Hi(V64) <> DWord($12345678) then
    Halt(16);

  U64 := QWord($FEDCBA9876543210);
  if Lo(U64) <> DWord($76543210) then
    Halt(17);
  if Hi(U64) <> DWord($FEDCBA98) then
    Halt(18);

  // A function call used as a statement discards its result.
  Lo(W);
  Hi(W);
  if W <> $1234 then
    Halt(19)
end.
