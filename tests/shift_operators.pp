program ShiftOperators;

{$R+}
{$Q+}

type
  TNibble = 0..15;

const
  VersionNr = '3';
  ReleaseNr = '2';
  PatchNr = '0';
  FoldedOrd = Ord(VersionNr);
  FoldedShift = (Ord(VersionNr) - Ord('0')) shl 14;
  WordVersion = ((Ord(VersionNr) - Ord('0')) shl 14) +
                ((Ord(ReleaseNr) - Ord('0')) shl 7) +
                (Ord(PatchNr) - Ord('0'));

var
  N: TNibble;
  B: Byte;
  SI: ShortInt;
  W: Word;
  SM: SmallInt;
  C: Cardinal;
  L: LongInt;
  I64: Int64;
  Q: QWord;
  Count: Integer;
  QCount, Count31, Count63: QWord;

function ResultKind(Value: Integer): Integer; overload;
begin
  if Value <> Value then
    Halt(90);
  Result := 1
end;

function ResultKind(Value: Cardinal): Integer; overload;
begin
  if Value <> Value then
    Halt(91);
  Result := 2
end;

function ResultKind(Value: Int64): Integer; overload;
begin
  if Value <> Value then
    Halt(92);
  Result := 3
end;

function ResultKind(Value: QWord): Integer; overload;
begin
  if Value <> Value then
    Halt(93);
  Result := 4
end;

function SubrangeKind(Value: Byte): Integer; overload;
begin
  if Value <> Value then
    Halt(94);
  Result := 1
end;

function SubrangeKind(Value: ShortInt): Integer; overload;
begin
  if Value <> Value then
    Halt(95);
  Result := 2
end;

begin
  if FoldedOrd <> 51 then Halt(40);
  if FoldedShift <> 49152 then Halt(41);
  if WordVersion <> 49408 then Halt(42);

  N := 1;
  B := 1;
  SI := 1;
  W := 1;
  SM := 1;
  C := 1;
  L := 1;
  I64 := 1;
  Q := 1;
  Count := 1;
  QCount := 1;

  { This is ordinary overload resolution, independent of shifts. TNibble is a
    distinct nominal type represented directly by its declared ShortInt base;
    passing it to Byte requires one assignment edge. }
  if SubrangeKind(N) <> 2 then Halt(39);

  { A typed variable count prevents constant folding from replacing these
    ordinary runtime operator calls. Integer exercises assignment into the
    common QWord count domain. }
  if ResultKind(N shl Count) <> 1 then Halt(1);
  if ResultKind(B shl Count) <> 2 then Halt(2);
  if ResultKind(SI shl Count) <> 1 then Halt(3);
  if ResultKind(W shl Count) <> 2 then Halt(4);
  if ResultKind(SM shl Count) <> 1 then Halt(5);
  if ResultKind(C shl Count) <> 2 then Halt(6);
  if ResultKind(L shl Count) <> 1 then Halt(7);
  if ResultKind(I64 shl Count) <> 3 then Halt(8);
  if ResultKind(Q shl Count) <> 4 then Halt(9);

  if ResultKind(N shr Count) <> 1 then Halt(10);
  if ResultKind(B shr Count) <> 2 then Halt(11);
  if ResultKind(SI shr Count) <> 1 then Halt(12);
  if ResultKind(W shr Count) <> 2 then Halt(13);
  if ResultKind(SM shr Count) <> 1 then Halt(14);
  if ResultKind(C shr Count) <> 2 then Halt(15);
  if ResultKind(L shr Count) <> 1 then Halt(16);
  if ResultKind(I64 shr Count) <> 3 then Halt(17);
  if ResultKind(Q shr Count) <> 4 then Halt(18);

  { An exact QWord count must leave selection to the independently typed
    value operand and therefore preserve every result carrier above. }
  if ResultKind(N shl QCount) <> 1 then Halt(43);
  if ResultKind(B shl QCount) <> 2 then Halt(44);
  if ResultKind(SI shl QCount) <> 1 then Halt(45);
  if ResultKind(W shl QCount) <> 2 then Halt(46);
  if ResultKind(SM shl QCount) <> 1 then Halt(47);
  if ResultKind(C shl QCount) <> 2 then Halt(48);
  if ResultKind(L shl QCount) <> 1 then Halt(49);
  if ResultKind(I64 shl QCount) <> 3 then Halt(50);
  if ResultKind(Q shl QCount) <> 4 then Halt(51);

  if ResultKind(N shr QCount) <> 1 then Halt(52);
  if ResultKind(B shr QCount) <> 2 then Halt(53);
  if ResultKind(SI shr QCount) <> 1 then Halt(54);
  if ResultKind(W shr QCount) <> 2 then Halt(55);
  if ResultKind(SM shr QCount) <> 1 then Halt(56);
  if ResultKind(C shr QCount) <> 2 then Halt(57);
  if ResultKind(L shr QCount) <> 1 then Halt(58);
  if ResultKind(I64 shr QCount) <> 3 then Halt(59);
  if ResultKind(Q shr QCount) <> 4 then Halt(60);

  if SizeOf(N shl Count) <> 4 then Halt(19);
  if SizeOf(L shl Count) <> 4 then Halt(20);
  if SizeOf(I64 shl Count) <> 8 then Halt(21);
  if SizeOf(Q shl Count) <> 8 then Halt(22);

  Count31 := 31;
  Count63 := 63;

  L := 1;
  if (L shl Count31) <> Low(LongInt) then Halt(24);

  I64 := 1;
  if (I64 shl Count63) <> Low(Int64) then Halt(30);

  L := -1;
  if (L shr Count) <> High(LongInt) then Halt(35);

  I64 := -1;
  if (I64 shr Count) <> High(Int64) then Halt(37)
end.
