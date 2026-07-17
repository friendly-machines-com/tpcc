program ImplicitRealNarrowing;

uses SysUtils;

type
  TRecordValue = record
    Value: Single;
  end;

const
{$ifdef CHECK_CONSTANT_REJECTION}
  {$R+}
  TooLarge: Single = 1.0e100;
{$else}
  Ordinary: Single = 1.5;
{$endif}

var
  S, OtherSingle: Single;
  D: Double;
  E: Extended;
  RecordValue: TRecordValue;
  Values: array[1..1] of Single;
  Caught: Boolean;

procedure TakeSingle(Value: Single);
begin
  S := Value
end;

function PreferNonNarrowing(
  Left: Double; Right: Single): Integer; overload;
begin
  if (Left = Left) and (Right = Right) then
    Result := 1
end;

function PreferNonNarrowing(
  Left, Right: Single): Integer; overload;
begin
  if (Left = Left) and (Right = Right) then
    Result := 2
end;

{$R+}
function CheckedResult(Value: Extended): Single;
begin
  Result := Value
end;

function CheckedExit(Value: Extended): Single;
begin
  Exit(Value)
end;

begin
{$ifdef CHECK_CONSTANT_REJECTION}
  if TooLarge = 0 then
    Halt(100)
{$else}
  {$R-}
  if Ordinary <> 1.5 then
    Halt(1);
  E := 1.0e100;
  S := E;
  if S <= High(Single) then
    Halt(2);
  E := -1.0e100;
  S := E;
  if S >= Low(Single) then
    Halt(3);

  E := High(Extended);
  D := E;
  if D <= High(Double) then
    Halt(4);

  {$R+}
  D := 1.5;
  OtherSingle := 2.5;
  if PreferNonNarrowing(D, OtherSingle) <> 1 then
    Halt(5);
  S := D;
  if S <> 1.5 then
    Halt(6);

  D := 1.0e-300;
  Caught := False;
  try
    S := D
  except
    on ERangeError do
      Caught := True
  end;
  if Caught or (S <> 0.0) then
    Halt(7);

  E := 1.0e100;
  Caught := False;
  try
    S := E
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(8);

  Caught := False;
  try
    RecordValue.Value := E
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(9);

  Caught := False;
  try
    Values[1] := E
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(10);

  Caught := False;
  try
    TakeSingle(E)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(11);

  Caught := False;
  try
    S := CheckedResult(E)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(12);

  Caught := False;
  try
    S := CheckedExit(E)
  except
    on ERangeError do
      Caught := True
  end;
  if not Caught then
    Halt(13);

  Caught := False;
  try
    S := Single(E)
  except
    on ERangeError do
      Caught := True
  end;
  if Caught or (S <= High(Single)) then
    Halt(14);

  Caught := False;
  try
    S := E as Single
  except
    on ERangeError do
      Caught := True
  end;
  if Caught or (S <= High(Single)) then
    Halt(15);

  {$R-}
  E := High(Extended);
  D := E;
  {$R+}
  Caught := False;
  try
    S := D
  except
    on ERangeError do
      Caught := True
  end;
  if Caught or (S <= High(Single)) then
    Halt(16);

  D := D - D;
  Caught := False;
  try
    S := D
  except
    on ERangeError do
      Caught := True
  end;
  if Caught or (S = S) then
    Halt(17)
{$endif}
end.
