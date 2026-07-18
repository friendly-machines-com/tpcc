program IncDecOperators;

uses SysUtils;

type
  TValue = record
    Data: Integer;
  end;

operator Inc(const Value: TValue): TValue;
begin
  Result := Value;
  Result.Data := Result.Data + 1
end;

operator UncheckedInc(const Value: TValue): TValue;
begin
  Result := Value;
  Result.Data := Result.Data + 10
end;

operator Dec(const Value: TValue): TValue;
begin
  Result := Value;
  Result.Data := Result.Data - 1
end;

operator UncheckedDec(const Value: TValue): TValue;
begin
  Result := Value;
  Result.Data := Result.Data - 10
end;

operator Add(const Value: TValue; Amount: Integer): TValue;
begin
  Result := Value;
  Result.Data := Result.Data + Amount
end;

operator UncheckedAdd(const Value: TValue; Amount: Integer): TValue;
begin
  Result := Value;
  Result.Data := Result.Data + Amount * 10
end;

operator Subtract(const Value: TValue; Amount: Integer): TValue;
begin
  Result := Value;
  Result.Data := Result.Data - Amount
end;

operator UncheckedSubtract(const Value: TValue; Amount: Integer): TValue;
begin
  Result := Value;
  Result.Data := Result.Data - Amount * 10
end;

type
  TSmall = 1..3;
  TTop = 2147483647..2147483647;
  TChoice = (First, Second, Third);
  TBaseChoice = (BaseFirst, BaseSecond, BaseThird);
  TChoiceSlice = BaseFirst..BaseSecond;
  TDistanceChoice = (DistanceFirst, DistanceSecond, DistanceThird);
  TIntegerPointer = ^Integer;
  TBoxPointer = ^TBox;
  TBox = object
  private
    FDirect: Integer;
    FItems: array[0..1] of Integer;
    function GetItem(Index: Integer): Integer;
    procedure SetItem(Index: Integer; Value: Integer);
  public
    property Direct: Integer
      read FDirect write FDirect;
    property Items[Index: Integer]: Integer
      read GetItem write SetItem; default;
  end;
  TPacked = packed record
    Value: Integer;
  end;
  TOverlay = packed record
    Value: Integer;
  end;
  TOverlayArray = packed record
    Values: array[0..1] of Integer;
  end;
  TWriteOnlyBox = object
  private
    FValue: Integer;
    procedure SetValue(Value: Integer);
  public
    property Value: Integer write SetValue;
  end;

operator Inc(Value: TBaseChoice): TBaseChoice;
begin
  if Ord(Value) = 0 then
    Result := BaseThird
  else
    Result := Value
end;

operator Add(Value: TDistanceChoice; Amount: Byte): TDistanceChoice;
begin
  if Amount = 1 then
    Result := DistanceFirst
  else
    Result := Value
end;

var
  Value: TValue;
  I: Integer;
  Small: TSmall;
  Top: TTop;
  Choice: TChoice;
  ChoiceSlice: TChoiceSlice;
  DistanceChoice: TDistanceChoice;
  DistanceAmount: Integer;
  Character: Char;
  Integers: array[0..3] of Integer;
  IntegerPointer: TIntegerPointer;
  Box: TBox;
  BoxLookups: Integer;
  PointerLookups: Integer;
  IndexLookups: Integer;
  GetterCalls: Integer;
  SetterCalls: Integer;
  PackedValue: TPacked;
  OverlayStorage: Integer;
  OverlayArrayStorage: Int64;
  WriteOnlyBox: TWriteOnlyBox;
  Caught: Boolean;

function TBox.GetItem(Index: Integer): Integer;
begin
  GetterCalls := GetterCalls + 1;
  Result := FItems[Index]
end;

procedure TBox.SetItem(Index: Integer; Value: Integer);
begin
  SetterCalls := SetterCalls + 1;
  FItems[Index] := Value
end;

procedure TWriteOnlyBox.SetValue(Value: Integer);
begin
  FValue := Value
end;

function FindBox: TBoxPointer;
begin
  BoxLookups := BoxLookups + 1;
  Result := @Box
end;

function FindIndex: Integer;
begin
  IndexLookups := IndexLookups + 1;
  Result := 1
end;

function FindInteger: TIntegerPointer;
begin
  PointerLookups := PointerLookups + 1;
  Result := @Integers[2]
end;

begin
  {$ifdef TEST_INC_NON_PLACE}
  Inc(1)
  {$else}
  {$ifdef TEST_INC_WRITE_ONLY}
  Inc(WriteOnlyBox.Value)
  {$else}
  Value.Data := 0;
  {$Q-}
  Inc(Value);
  {$Q+}
  Inc(Value);
  {$Q-}
  Inc(Value, 2);
  {$Q+}
  Inc(Value, 2);
  {$Q-}
  Dec(Value);
  {$Q+}
  Dec(Value);
  {$Q-}
  Dec(Value, 2);
  {$Q+}
  Dec(Value, 2);
  if Value.Data <> 0 then
    Halt(1);

  {$Q+}
  {$R-}
  ChoiceSlice := BaseFirst;
  Inc(ChoiceSlice);
  if Ord(ChoiceSlice) <> 2 then
    Halt(35);

  DistanceChoice := DistanceSecond;
  DistanceAmount := 1;
  Inc(DistanceChoice, DistanceAmount);
  if Ord(DistanceChoice) <> 0 then
    Halt(36);

  {$Q-}
  I := High(Integer);
  Inc(I);
  if I <> Low(Integer) then
    Halt(2);
  I := Low(Integer);
  Dec(I);
  if I <> High(Integer) then
    Halt(3);

  {$Q+}
  Caught := False;
  try
    I := High(Integer);
    Inc(I)
  except
    on EIntOverflow do
      Caught := True
  end;
  if not Caught then
    Halt(4);

  Caught := False;
  try
    I := Low(Integer);
    Dec(I)
  except
    on EIntOverflow do
      Caught := True
  end;
  if not Caught then
    Halt(5);

  {$R+}
  Caught := False;
  try
    Top := 2147483647;
    Inc(Top)
  except
    on EIntOverflow do
      Caught := True
  end;
  if not Caught then
    Halt(37);

  {$Q-}
  Caught := False;
  try
    Top := 2147483647;
    Inc(Top)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(38);

  Small := 3;
  Caught := False;
  try
    Inc(Small)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(6);

  Choice := Third;
  Caught := False;
  try
    Inc(Choice)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(7);

  Choice := First;
  Caught := False;
  try
    Dec(Choice)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(39);

  {$R-}
  Small := 3;
  Inc(Small);
  if Integer(Small) <> 4 then
    Halt(8);
  Choice := Third;
  Inc(Choice);
  if Ord(Choice) <> 3 then
    Halt(9);
  Choice := First;
  Dec(Choice);
  if Ord(Choice) <> 4294967295 then
    Halt(40);

  Character := 'a';
  Inc(Character, 2);
  if Character <> 'c' then
    Halt(10);
  Character := Char(255);
  Inc(Character);
  if Ord(Character) <> 0 then
    Halt(11);

  {$Q+}
  Caught := False;
  try
    Character := Char(255);
    Inc(Character)
  except
    on EIntOverflow do
      Caught := True
  end;
  if not Caught then
    Halt(12);

  IntegerPointer := @Integers[0];
  Inc(IntegerPointer);
  if IntegerPointer <> @Integers[1] then
    Halt(13);
  Inc(IntegerPointer, 2);
  if IntegerPointer <> @Integers[3] then
    Halt(14);
  Dec(IntegerPointer, 2);
  Dec(IntegerPointer);
  if IntegerPointer <> @Integers[0] then
    Halt(15);

  Integers[2] := 39;
  PointerLookups := 0;
  Inc(FindInteger()^);
  if Integers[2] <> 40 then
    Halt(19);
  if PointerLookups <> 1 then
    Halt(20);

  Box.Items[1] := 40;
  BoxLookups := 0;
  IndexLookups := 0;
  GetterCalls := 0;
  SetterCalls := 0;
  Inc(FindBox()^.Items[FindIndex()]);
  if Box.Items[1] <> 41 then
    Halt(21);
  if BoxLookups <> 1 then
    Halt(22);
  if IndexLookups <> 1 then
    Halt(23);
  if GetterCalls <> 2 then
    Halt(24);
  if SetterCalls <> 1 then
    Halt(25);

  Box.Direct := 45;
  Inc(Box.Direct);
  if Box.Direct <> 46 then
    Halt(26);

  Integers[1] := 47;
  IndexLookups := 0;
  Inc(Integers[FindIndex()]);
  if Integers[1] <> 48 then
    Halt(27);
  if IndexLookups <> 1 then
    Halt(28);

  Character := #0;
  Inc(Byte(Character));
  if Ord(Character) <> 1 then
    Halt(29);

  PackedValue.Value := 50;
  Inc(PackedValue.Value);
  if PackedValue.Value <> 51 then
    Halt(30);

  OverlayStorage := 60;
  Inc(TOverlay(OverlayStorage).Value);
  if OverlayStorage <> 61 then
    Halt(31);

  OverlayArrayStorage := 0;
  TOverlayArray(OverlayArrayStorage).Values[1] := 70;
  IndexLookups := 0;
  Inc(TOverlayArray(OverlayArrayStorage).Values[FindIndex()]);
  if TOverlayArray(OverlayArrayStorage).Values[1] <> 71 then
    Halt(32);
  if IndexLookups <> 1 then
    Halt(33)
  {$endif}
  {$endif}
end.
