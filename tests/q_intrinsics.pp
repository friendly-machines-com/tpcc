program QIntrinsics;

uses SysUtils;

type
  TSmall = 1..3;
  TNegative = -3..-1;
  TChoice = (First, Second, Third);
  TBox = record
    Value: Integer;
  end;

function Abs(Value: TBox): TBox; overload;
begin
  Result := Value;
  Result.Value := Result.Value + 10
end;

var
  B: Byte;
  SB: ShortInt;
  W: Word;
  SW: SmallInt;
  C: Cardinal;
  I: Integer;
  L: LongInt;
  Q: QWord;
  I64: Int64;
  PI: PtrInt;
  PU: PtrUInt;
  SI: SizeInt;
  SU: SizeUInt;
  F: Single;
  D: Double;
  E: Extended;
  Ch: Char;
  Bool: Boolean;
  Small: TSmall;
  Negative: TNegative;
  Choice: TChoice;
  Box: TBox;
  Caught: Boolean;

begin
  {$Q-}
  B := Abs(1);
  if B <> 1 then Halt(53);
  B := Succ(1);
  if B <> 2 then Halt(54);
  B := Pred(1);
  if B <> 0 then Halt(55);
  B := Succ(High(Byte));
  if B <> Low(Byte) then Halt(1);
  B := Pred(Low(Byte));
  if B <> High(Byte) then Halt(2);
  SB := Succ(High(ShortInt));
  if SB <> Low(ShortInt) then Halt(3);
  SB := Pred(Low(ShortInt));
  if SB <> High(ShortInt) then Halt(4);
  W := Succ(High(Word));
  if W <> Low(Word) then Halt(5);
  W := Pred(Low(Word));
  if W <> High(Word) then Halt(6);
  SW := Succ(High(SmallInt));
  if SW <> Low(SmallInt) then Halt(7);
  SW := Pred(Low(SmallInt));
  if SW <> High(SmallInt) then Halt(8);
  C := Succ(High(Cardinal));
  if C <> Low(Cardinal) then Halt(9);
  C := Pred(Low(Cardinal));
  if C <> High(Cardinal) then Halt(10);
  I := Succ(High(Integer));
  if I <> Low(Integer) then Halt(11);
  I := Pred(Low(Integer));
  if I <> High(Integer) then Halt(12);
  Q := Succ(High(QWord));
  if Q <> Low(QWord) then Halt(13);
  Q := Pred(Low(QWord));
  if Q <> High(QWord) then Halt(14);
  I64 := Succ(High(Int64));
  if I64 <> Low(Int64) then Halt(15);
  I64 := Pred(Low(Int64));
  if I64 <> High(Int64) then Halt(16);
  L := Succ(High(LongInt));
  if L <> Low(LongInt) then Halt(40);
  PI := Succ(High(PtrInt));
  if PI <> Low(PtrInt) then Halt(41);
  PU := Pred(Low(PtrUInt));
  if PU <> High(PtrUInt) then Halt(42);
  SI := Succ(High(SizeInt));
  if SI <> Low(SizeInt) then Halt(43);
  SU := Pred(Low(SizeUInt));
  if SU <> High(SizeUInt) then Halt(44);
  Ch := Succ(High(Char));
  if Ord(Ch) <> 0 then Halt(17);
  Ch := Pred(Low(Char));
  if Ord(Ch) <> 255 then Halt(18);
  I := Abs(Low(Integer));
  if I <> Low(Integer) then Halt(19);
  I64 := Abs(Low(Int64));
  if I64 <> Low(Int64) then Halt(20);
  F := -2.0;
  D := -3.0;
  E := -4.0;
  if Abs(F) <> 2.0 then Halt(45);
  if Abs(D) <> 3.0 then Halt(46);
  if Abs(E) <> 4.0 then Halt(47);

  {$Q+}
  Caught := False;
  try
    B := Succ(High(Byte))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(21);
  Caught := False;
  try
    SB := Pred(Low(ShortInt))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(22);
  Caught := False;
  try
    W := Succ(High(Word))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(23);
  Caught := False;
  try
    SW := Pred(Low(SmallInt))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(24);
  Caught := False;
  try
    C := Succ(High(Cardinal))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(25);
  Caught := False;
  try
    I := Pred(Low(Integer))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(26);
  Caught := False;
  try
    Q := Succ(High(QWord))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(27);
  Caught := False;
  try
    I64 := Pred(Low(Int64))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(28);
  Caught := False;
  try
    Ch := Pred(Low(Char))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(29);
  Caught := False;
  try
    I := Abs(Low(Integer))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(30);
  Caught := False;
  try
    I64 := Abs(Low(Int64))
  except
    on EIntOverflow do Caught := True
  end;
  if not Caught then Halt(31);
  F := -2.0;
  D := -3.0;
  E := -4.0;
  if Abs(F) <> 2.0 then Halt(48);
  if Abs(D) <> 3.0 then Halt(49);
  if Abs(E) <> 4.0 then Halt(50);

  { $Q checks the carrier. $R independently checks the declared Pascal
    enum/subrange bounds when the exact-T result is formed. }
  {$Q+}
  {$R-}
  Small := 3;
  Small := Succ(Small);
  if Ord(Small) <> 4 then Halt(32);
  Negative := -2;
  Negative := Abs(Negative);
  if Ord(Negative) <> 2 then Halt(33);
  Choice := Third;
  Choice := Succ(Choice);
  if Ord(Choice) <> 3 then Halt(34);
  Choice := First;
  Choice := Pred(Choice);
  if Ord(Choice) <> High(Cardinal) then Halt(35);
  Bool := Succ(True);
  if Ord(Bool) <> 2 then Halt(51);

  {$R+}
  Caught := False;
  try
    Small := 3;
    Small := Succ(Small)
  except
    on ERangeError do Caught := True
  end;
  if not Caught then Halt(36);
  Caught := False;
  try
    Negative := -2;
    Negative := Abs(Negative)
  except
    on ERangeError do Caught := True
  end;
  if not Caught then Halt(37);
  Caught := False;
  try
    Choice := Third;
    Choice := Succ(Choice)
  except
    on ERangeError do Caught := True
  end;
  if not Caught then Halt(38);
  Caught := False;
  try
    Bool := Succ(True)
  except
    on ERangeError do Caught := True
  end;
  if not Caught then Halt(52);

  { A complete typed user overload wins through ordinary lookup under both
    directive states. It is not rewritten into the compiler fallback. }
  {$Q-}
  Box.Value := 1;
  Box := Abs(Box);
  {$Q+}
  Box := Abs(Box);
  if Box.Value <> 21 then Halt(39);

  { Both loop directions are emitted under each directive state. The terminal
    guard means a valid loop never performs a synthetic overflowing step. }
  {$Q-}
  for I := 1 to 2 do
    B := B;
  for I := 2 downto 1 do
    B := B;
  {$Q+}
  for I := 1 to 2 do
    B := B;
  for I := 2 downto 1 do
    B := B
end.
