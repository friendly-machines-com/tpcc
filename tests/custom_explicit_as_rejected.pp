program CustomExplicitAsRejected;

type
  TSource = record
    Value: Integer;
  end;
  TTarget = record
    Value: Integer;
  end;

operator Explicit(const Value: TSource): TTarget;
begin
  Result.Value := Value.Value
end;

var
  Source: TSource;
  Target: TTarget;
begin
  Target := Source as TTarget
end.
