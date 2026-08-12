program SwapEndianBuiltin;

var
  S16, OriginalS16: SmallInt;
  U16, OriginalU16: Word;
  S32, OriginalS32: LongInt;
  U32, OriginalU32: DWord;
  S64, OriginalS64: Int64;
  U64, OriginalU64: QWord;

begin
  S16 := SmallInt($80ff);
  OriginalS16 := S16;
  if SwapEndian(S16) <> SmallInt($ff80) then
    Halt(1);
  if S16 <> OriginalS16 then
    Halt(2);

  U16 := $1234;
  OriginalU16 := U16;
  if SwapEndian(U16) <> Word($3412) then
    Halt(3);
  if U16 <> OriginalU16 then
    Halt(4);

  S32 := LongInt($89abcdef);
  OriginalS32 := S32;
  if SwapEndian(S32) <> LongInt($efcdab89) then
    Halt(5);
  if S32 <> OriginalS32 then
    Halt(6);

  U32 := $89abcdef;
  OriginalU32 := U32;
  if SwapEndian(U32) <> DWord($efcdab89) then
    Halt(7);
  if U32 <> OriginalU32 then
    Halt(8);

  S64 := Int64($0123456789abcdef);
  OriginalS64 := S64;
  if SwapEndian(S64) <> Int64($efcdab8967452301) then
    Halt(9);
  if S64 <> OriginalS64 then
    Halt(10);

  U64 := $0123456789abcdef;
  OriginalU64 := U64;
  if SwapEndian(U64) <> QWord($efcdab8967452301) then
    Halt(11);
  if U64 <> OriginalU64 then
    Halt(12);

  if SwapEndian(SwapEndian(U64)) <> U64 then
    Halt(13);

  // A function call used as a statement discards its result.
  SwapEndian(U64);
  if U64 <> OriginalU64 then
    Halt(14)
end.
