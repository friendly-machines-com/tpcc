program ImplicitConversionContractMismatchRejected;

type
  TSource = record
    Value: Integer;
  end;
  TDestination = record
    Value: Integer;
  end;

{$ifdef REVERSE}
operator Implicit(
  const Value: TSource): TDestination; forward;

operator :=(
  const Value: TSource): TDestination;
{$else}
operator :=(
  const Value: TSource): TDestination; forward;

operator Implicit(
  const Value: TSource): TDestination;
{$endif}
begin
  Result.Value := Value.Value
end;

begin
end.
