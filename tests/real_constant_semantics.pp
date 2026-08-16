program RealConstantSemantics;

const
  ExactOrigin = 1000.0;
  ExactAlias = ExactOrigin;
  RoundedOrigin = 0.1;
  RoundedAlias = RoundedOrigin;
  X = 5.73;
  Y = 0.1;
  Z = X + Y;
  SingleBoundary = Single(16777216.0) + Single(1.0);
  DoubleBoundary = Double(9007199254740992.0) + Double(1.0);
  NegativeZero = Single(-0.0);
  PositiveExponent = 1.25e+2;
  MinimumIntegerOrigin = -9223372036854775808;
  MaximumIntegerOrigin = 18446744073709551615;

var
  ExactRank: Integer;
  ExactAliasRank: Integer;
  RoundedRank: Integer;
  RoundedAliasRank: Integer;
  RoundedIntegerRank: Integer;
  WideIntegerRank: Integer;
  WideIntegerMixedRank: Integer;
  FoldedRank: Integer;
  ExactMixedRank: Integer;
  RoundedMixedRank: Integer;
  AddRank: Integer;
  MultiplyRank: Integer;
  DivideRank: Integer;
  MixedLess: Boolean;
  SingleValue: Single;
  Milliseconds: Word;
  FoldedZ: Extended;
  RuntimeX: Extended;
  RuntimeY: Extended;
  RuntimeZ: Extended;
  FoldedSingleBoundary: Single;
  FoldedDoubleBoundary: Double;
  FoldedNegativeZero: Single;
  FoldedPositiveExponent: Single;
  FoldedMinimumInteger: Int64;
  FoldedMaximumInteger: QWord;
  ExactOriginSize: SizeInt;
  RoundedOriginSize: SizeInt;
  TinySingle: Single;
  TinyDouble: Double;
  TinyExtended: Extended;

function Domain(Value: Single): Integer; overload;
begin
  if Value = Value then
    Result := 1
end;

function Domain(Value: Double): Integer; overload;
begin
  if Value = Value then
    Result := 2
end;

function Domain(Value: Extended): Integer; overload;
begin
  if Value = Value then
    Result := 3
end;

{ These bodies are not executed. They verify that R+ and R- retain the same
  selected Single overload even when the origin exceeds every real domain;
  only the constructed conversion/check differs. }
procedure CompileUncheckedHugeOrigin;
begin
  {$R-}
  ExactRank := Domain(1e5000)
end;

procedure CompileCheckedHugeOrigin;
begin
  {$R+}
  ExactRank := Domain(1e5000)
end;

begin
  ExactRank := Domain(ExactOrigin);
  ExactAliasRank := Domain(ExactAlias);
  RoundedRank := Domain(RoundedOrigin);
  RoundedAliasRank := Domain(RoundedAlias);
  RoundedIntegerRank := Domain(16777217);
  WideIntegerRank := Domain(MaximumIntegerOrigin);
  WideIntegerMixedRank := Domain(MaximumIntegerOrigin + 0.5);
  FoldedRank := Domain(Z);

  SingleValue := 2.0;
  ExactMixedRank := Domain(SingleValue + 0.5);
  RoundedMixedRank := Domain(SingleValue + 0.1);

  Milliseconds := 500;
  AddRank := Domain(Milliseconds + 1000.0);
  MultiplyRank := Domain(Milliseconds * 1000.0);
  DivideRank := Domain(Milliseconds / 1000.0);
  MixedLess := Milliseconds < 1000.0;

  FoldedZ := Z;
  RuntimeX := X;
  RuntimeY := Y;
  RuntimeZ := RuntimeX + RuntimeY;
  FoldedSingleBoundary := SingleBoundary;
  FoldedDoubleBoundary := DoubleBoundary;
  FoldedNegativeZero := NegativeZero;
  FoldedPositiveExponent := PositiveExponent;
  FoldedMinimumInteger := MinimumIntegerOrigin;
  FoldedMaximumInteger := MaximumIntegerOrigin;
  ExactOriginSize := SizeOf(1.0);
  RoundedOriginSize := SizeOf(0.1);
  TinySingle := Single(1e-1000);
  TinyDouble := Double(1e-1000);
  TinyExtended := Extended(1e-5000)
end.
