program CustomInAmbiguous;

{$R+}

type
  TSource = record
    Value: Integer;
  end;
  TChoiceA = record
    Value: Integer;
  end;
  TChoiceB = record
    Value: Integer;
  end;
  TIntegerSet = set of Integer;

operator Implicit(
  const Value: TSource): TChoiceA;
begin
  Result.Value := Value.Value
end;

operator Implicit(
  const Value: TSource): TChoiceB;
begin
  Result.Value := Value.Value
end;

operator Implicit(
  const Value: TSource): Integer;
begin
  Result := Value.Value
end;

operator In(
  const Item: TChoiceA;
  const Values: TIntegerSet): Boolean;
begin
  Result := Item.Value = 0
end;

operator In(
  const Item: TChoiceB;
  const Values: TIntegerSet): Boolean;
begin
  Result := Item.Value = 0
end;

var
  Source: TSource;
  Values: TIntegerSet;
  Hit: Boolean;

begin
  Hit := Source in Values
end.
