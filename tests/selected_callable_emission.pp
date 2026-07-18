program SelectedCallableEmission;

type
  TIntegerSet = set of Integer;

var
  Selected: Integer;
  Values: TIntegerSet;
  Hit: Boolean;

operator In(
  const Item: Integer;
  const SetValue: TIntegerSet): Boolean;
var
  CopyOfSet: TIntegerSet;
begin
  CopyOfSet := SetValue;
  Exclude(CopyOfSet, Item);
  Selected := Item;
  Result := False
end;

begin
  Values := [9];
  Selected := 0;
  Hit := 9 in Values;
  if (Selected <> 9) or
     Hit then
    Halt(1)
end.
