program HexStrBuiltin;

var
  L: LongInt;
  I: Int64;
  Q: QWord;
  P: Pointer;
  S: ShortString;
  Position: Integer;

begin
  L := 305441741;
  if HexStr(L, 8) <> '1234ABCD' then
    Halt(1);
  if HexStr(L, 4) <> 'ABCD' then
    Halt(2);
  if HexStr(L, 10) <> '001234ABCD' then
    Halt(3);

  L := -1;
  if HexStr(L, 10) <> '00FFFFFFFF' then
    Halt(4);

  I := 81985529216486895;
  if HexStr(I, 16) <> '0123456789ABCDEF' then
    Halt(5);
  I := -2;
  if HexStr(I, 4) <> 'FFFE' then
    Halt(6);

  Q := 18364758544493064720;
  if HexStr(Q, 16) <> 'FEDCBA9876543210' then
    Halt(7);
  if HexStr(Q, 18) <> '00FEDCBA9876543210' then
    Halt(8);
  if HexStr(Q, 0) <> '' then
    Halt(9);

  L := 0;
  S := HexStr(L, 255);
  if Length(S) <> 255 then
    Halt(10);
  for Position := 1 to Length(S) do
    if S[Position] <> '0' then
      Halt(11);

  P := nil;
  S := HexStr(P);
  if Length(S) <> SizeOf(Pointer) * 2 then
    Halt(12);
  for Position := 1 to Length(S) do
    if S[Position] <> '0' then
      Halt(13)
end.
