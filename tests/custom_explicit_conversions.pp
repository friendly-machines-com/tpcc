program CustomExplicitConversions;

type
  TSource = record
    Value: Integer;
  end;
  TImplicitSource = record
    Value: Integer;
  end;
  TUncheckedSource = record
    Value: Integer;
  end;
  TBothSource = record
    Value: Integer;
  end;
  TMultiSource = record
    Value: Integer;
  end;
  TExplicitOnlySource = record
    Value: Integer;
  end;
  TStrongExtended = type Extended;
  TCompLike = record
    Value: Int64;
  end;
  TTarget = record
    Value: Integer;
  end;
  TOtherTarget = record
    Value: Integer;
  end;

var
  Source: TSource;
  ImplicitSource: TImplicitSource;
  UncheckedSource: TUncheckedSource;
  BothSource: TBothSource;
  MultiSource: TMultiSource;
  ExplicitOnlySource: TExplicitOnlySource;
  Target: TTarget;
  OtherTarget: TOtherTarget;
  StrongExtended: TStrongExtended;
  CompLike: TCompLike;
  Selected: Integer;
  Small: Byte;

operator Explicit(const Value: TSource): TTarget;
begin
  Selected := 1;
  Result.Value := Value.Value + 100
end;

operator Implicit(const Value: TSource): TTarget;
begin
  Selected := 2;
  Result.Value := Value.Value + 200
end;

operator UncheckedImplicit(const Value: TSource): TTarget;
begin
  Selected := 3;
  Result.Value := Value.Value + 300
end;

operator Implicit(
  const Value: TImplicitSource): TTarget;
begin
  Selected := 4;
  Result.Value := Value.Value + 400
end;

operator UncheckedImplicit(
  const Value: TUncheckedSource): TTarget;
begin
  Selected := 5;
  Result.Value := Value.Value + 500
end;

operator Implicit(
  const Value: TBothSource): TTarget;
begin
  Selected := 6;
  Result.Value := Value.Value + 600
end;

operator UncheckedImplicit(
  const Value: TBothSource): TTarget;
begin
  Selected := 7;
  Result.Value := Value.Value + 700
end;

operator Explicit(
  const Value: TMultiSource): TTarget;
begin
  Selected := 8;
  Result.Value := Value.Value + 800
end;

operator Explicit(
  const Value: TMultiSource): TOtherTarget;
begin
  Selected := 9;
  Result.Value := Value.Value + 900
end;

operator Explicit(
  const Value: TExplicitOnlySource): TTarget;
begin
  Selected := 10;
  Result.Value := Value.Value + 1000
end;

operator Explicit(const Value: Extended): TCompLike;
begin
  Selected := 11;
  Result.Value := Round(Value)
end;

{ An ordinary function with this source name is not an operator declaration
  and must remain in the ordinary Pascal/C++ namespace. }
function Explicit(Value: Integer): Integer;
begin
  Result := Value + 10000
end;

begin
  Source.Value := 1;
  {$R-}
  Selected := 0;
  Target := TTarget(Source);
  if (Selected <> 1) or
     (Target.Value <> 101) then
    Halt(1);

  {$R+}
  Selected := 0;
  Target := TTarget(Source);
  if (Selected <> 1) or
     (Target.Value <> 101) then
    Halt(2);

  ImplicitSource.Value := 2;
  {$R-}
  Selected := 0;
  Target := TTarget(ImplicitSource);
  if (Selected <> 4) or
     (Target.Value <> 402) then
    Halt(3);

  UncheckedSource.Value := 3;
  {$R+}
  Selected := 0;
  Target := TTarget(UncheckedSource);
  if (Selected <> 5) or
     (Target.Value <> 503) then
    Halt(4);

  BothSource.Value := 4;
  {$R-}
  Selected := 0;
  Target := TTarget(BothSource);
  if (Selected <> 6) or
     (Target.Value <> 604) then
    Halt(5);

  {$R+}
  Selected := 0;
  Target := TTarget(BothSource);
  if (Selected <> 6) or
     (Target.Value <> 604) then
    Halt(6);

  MultiSource.Value := 5;
  Selected := 0;
  Target := TTarget(MultiSource);
  if (Selected <> 8) or
     (Target.Value <> 805) then
    Halt(7);
  Selected := 0;
  OtherTarget := TOtherTarget(MultiSource);
  if (Selected <> 9) or
     (OtherTarget.Value <> 905) then
    Halt(8);

  ExplicitOnlySource.Value := 6;
  Selected := 0;
  Target := TTarget(ExplicitOnlySource);
  if (Selected <> 10) or
     (Target.Value <> 1006) then
    Halt(9);

  { No conversion operator applies, so this remains the predefined explicit
    ordinal cast and retains its defined truncation behavior. }
  Small := Byte(300);
  if Small <> 44 then
    Halt(10);

  if Explicit(7) <> 10007 then
    Halt(11);

  { A `type Base` actual needs no value conversion to enter a Base formal.
    The distinct identity remains relevant to overload selection, but the
    shared carrier must not make this look like an A -> B -> C chain. }
  StrongExtended := 12.0;
  Selected := 0;
  CompLike := TCompLike(StrongExtended);
  if (Selected <> 11) or
     (CompLike.Value <> 12) then
    Halt(12);

  {$ifdef REJECT_EXPLICIT_AS_IMPLICIT}
  Target := ExplicitOnlySource;
  {$endif}
end.
