program ShiftOperators;

{$R+}
{$Q+}

type
  TNibble = 0..15;

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
  Count, NegativeCount, Count31, Count32, Count33, Count63, Count64,
    Count65: Integer;

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

  { This is ordinary overload resolution, independent of shifts. TNibble is a
    distinct nominal type represented directly by its declared ShortInt base;
    passing it to Byte requires one assignment edge. }
  if SubrangeKind(N) <> 2 then Halt(39);

  { A variable count prevents FPC-style constant folding from shrinking the
    result to the folded literal's natural carrier. These checks exercise the
    ordinary runtime operator families. }
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

  if SizeOf(N shl Count) <> 4 then Halt(19);
  if SizeOf(L shl Count) <> 4 then Halt(20);
  if SizeOf(I64 shl Count) <> 8 then Halt(21);
  if SizeOf(Q shl Count) <> 8 then Halt(22);

  NegativeCount := -1;
  Count31 := 31;
  Count32 := 32;
  Count33 := 33;
  Count63 := 63;
  Count64 := 64;
  Count65 := 65;

  L := 1;
  if (L shl NegativeCount) <> Low(LongInt) then Halt(23);
  if (L shl Count31) <> Low(LongInt) then Halt(24);
  if (L shl Count32) <> 1 then Halt(25);
  if (L shl Count33) <> 2 then Halt(26);

  C := 1;
  if (C shl Count32) <> 1 then Halt(27);
  if (C shl Count33) <> 2 then Halt(28);

  I64 := 1;
  if (I64 shl NegativeCount) <> Low(Int64) then Halt(29);
  if (I64 shl Count63) <> Low(Int64) then Halt(30);
  if (I64 shl Count64) <> 1 then Halt(31);
  if (I64 shl Count65) <> 2 then Halt(32);

  Q := 1;
  if (Q shl Count64) <> 1 then Halt(33);
  if (Q shl Count65) <> 2 then Halt(34);

  L := -1;
  if (L shr Count) <> High(LongInt) then Halt(35);
  if (L shr Count32) <> -1 then Halt(36);

  I64 := -1;
  if (I64 shr Count) <> High(Int64) then Halt(37);
  if (I64 shr Count64) <> -1 then Halt(38)
end.
