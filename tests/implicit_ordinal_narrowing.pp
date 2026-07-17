program ImplicitOrdinalNarrowing;

uses SysUtils;

type
  TNarrow = 1..10;
  TWide = 1..20;
  TShade = (ShadeDark, ShadeMid, ShadeLight);
  TShadeRange = ShadeDark..ShadeMid;
  TLetterRange = 'a'..'m';
  TRecordValue = record
    Value: Byte;
  end;
  TPackedValue = packed record
    Value: Byte;
  end;
  TBox = object
  private
    FValue: Byte;
    procedure SetValue(NewValue: Byte);
  public
    property Value: Byte read FValue write SetValue;
  end;

var
  B: Byte;
  W: Word;
  I: Integer;
  C: Cardinal;
  Signed64, MixedInt64: Int64;
  Unsigned64, MixedQWord: QWord;
  Narrow: TNarrow;
  Wide: TWide;
  Shade: TShade;
  ShadeRange: TShadeRange;
  Character: Char;
  Letter: TLetterRange;
  RecordValue: TRecordValue;
  PackedValue: TPackedValue;
  Box: TBox;
  Values: array[1..1] of Byte;
  Bytes: set of Byte;
  Caught: Boolean;

procedure TBox.SetValue(NewValue: Byte);
begin
  FValue := NewValue
end;

procedure TakeByte(Value: Byte);
begin
  B := Value
end;

procedure TakeConstByte(const Value: Byte);
begin
  B := Value
end;

function PreferNonNarrowing(
  Left, Right: Cardinal): Integer; overload;
begin
  if (Left = Left) and (Right = Right) then
    Result := 1
end;

function PreferNonNarrowing(
  Left, Right: Byte): Integer; overload;
begin
  if (Left = Left) and (Right = Right) then
    Result := 2
end;

{$R+}
function CheckedResult(Value: Word): Byte;
begin
  Result := Value
end;

function CheckedExit(Value: Word): Byte;
begin
  Exit(Value)
end;

begin
  {$R-}
  W := 300;
  B := W;
  if B <> 44 then
    Halt(1);
  I := -1;
  C := I;
  if C <> High(Cardinal) then
    Halt(2);
  Wide := 15;
  Narrow := Wide;
  if Ord(Narrow) <> 15 then
    Halt(3);
  Shade := ShadeLight;
  ShadeRange := Shade;
  if Ord(ShadeRange) <> 2 then
    Halt(4);
  Character := 'z';
  Letter := Character;
  if Ord(Letter) <> Ord('z') then
    Halt(5);
  { Promotion preference chooses the integer carrier; runtime range checking
    independently governs the signed/unsigned operand conversion for the call. }
  Signed64 := 0;
  Unsigned64 := High(QWord);
  MixedInt64 := Signed64 + Unsigned64;
  if MixedInt64 <> -1 then
    Halt(22);
  I := -1;
  Unsigned64 := 2;
  MixedQWord := Unsigned64 + I;
  if MixedQWord <> 1 then
    Halt(23);

  {$R+}
  C := 15;
  if PreferNonNarrowing(C, 15) <> 1 then
    Halt(6);
  W := 7;
  Narrow := W;
  if Narrow <> 7 then
    Halt(7);

  W := 300;
  Caught := False;
  try
    B := W
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(8);

  Caught := False;
  try
    RecordValue.Value := W
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(9);

  Caught := False;
  try
    PackedValue.Value := W
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(10);

  Caught := False;
  try
    Values[1] := W
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(11);

  Caught := False;
  try
    Box.Value := W
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(12);

  Caught := False;
  try
    TakeByte(W)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(13);

  Caught := False;
  try
    TakeConstByte(W)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(14);

  Caught := False;
  try
    B := CheckedResult(W)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(15);

  Caught := False;
  try
    B := CheckedExit(W)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(16);

  Wide := 15;
  Caught := False;
  try
    Narrow := Wide
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(17);

  Shade := ShadeLight;
  Caught := False;
  try
    ShadeRange := Shade
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(18);

  Character := 'z';
  Caught := False;
  try
    Letter := Character
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(19);

  Caught := False;
  try
    Bytes := [W]
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(20);

  Bytes := [];
  Caught := False;
  try
    Include(Bytes, W)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(21);

  Signed64 := 0;
  Unsigned64 := High(QWord);
  Caught := False;
  try
    MixedInt64 := Signed64 + Unsigned64
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(24);

  I := -1;
  Unsigned64 := 2;
  Caught := False;
  try
    MixedQWord := Unsigned64 + I
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(25)
end.
