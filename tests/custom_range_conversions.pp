program CustomRangeConversions;

type
  TSource = record
    Value: Integer;
  end;
  TDestination = record
    Value: Integer;
  end;
  TLegacySource = record
    Value: Integer;
  end;
  TLegacyDestination = record
    Value: Integer;
  end;

var
  Source: TSource;
  Destination: TDestination;
  LegacySource: TLegacySource;
  LegacyDestination: TLegacyDestination;
  Selected: Integer;

{$ifndef OMIT_UNCHECKED}
operator UncheckedImplicit(
  const Value: TSource): TDestination;
begin
  Selected := 1;
  Result.Value := Value.Value + 100
end;
{$endif}

{$ifndef OMIT_CHECKED}
operator Implicit(
  const Value: TSource): TDestination;
begin
  Selected := 2;
  Result.Value := Value.Value + 200
end;
{$endif}

operator :=(
  const Value: TLegacySource): TLegacyDestination;
begin
  Selected := Selected + 10;
  Result.Value := Value.Value + 300
end;

function TakeDestination(
  Value: TDestination): Integer;
begin
  Result := Value.Value
end;

{ These ordinary routines deliberately share source names with the operator
  declarations. They remain ordinary Pascal identifiers and must not acquire
  the conversion destination-tag ABI. }
function Implicit(Value: Integer): Integer;
begin
  Result := Value + 1000
end;

function UncheckedImplicit(
  Value: Integer): Integer;
begin
  Result := Value + 2000
end;

begin
  Source.Value := 1;

  {$R-}
  Selected := 0;
  Destination := Source;
  if Selected <> 1 then
    Halt(1);
  if Destination.Value <> 101 then
    Halt(2);
  Selected := 0;
  if TakeDestination(Source) <> 101 then
    Halt(3);
  if Selected <> 1 then
    Halt(4);

  {$R+}
  Selected := 0;
  Destination := Source;
  if Selected <> 2 then
    Halt(5);
  if Destination.Value <> 201 then
    Halt(6);
  Selected := 0;
  if TakeDestination(Source) <> 201 then
    Halt(7);
  if Selected <> 2 then
    Halt(8);

  LegacySource.Value := 2;
  {$R-}
  Selected := 0;
  LegacyDestination := LegacySource;
  if Selected <> 10 then
    Halt(9);
  if LegacyDestination.Value <> 302 then
    Halt(10);

  {$R+}
  LegacyDestination := LegacySource;
  if Selected <> 20 then
    Halt(11);
  if LegacyDestination.Value <> 302 then
    Halt(12);

  if Implicit(3) <> 1003 then
    Halt(13);
  if UncheckedImplicit(4) <> 2004 then
    Halt(14)
end.
