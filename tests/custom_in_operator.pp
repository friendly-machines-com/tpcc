program CustomInOperator;

type
  TContainer = record
    Value: Integer;
  end;
  TIntegerSet = set of Integer;
  TCharSet = set of Char;

var
  Selected: Integer;
  Container: TContainer;
  IntegerValues: TIntegerSet;
  CharacterValues: TCharSet;

operator In(
  const Item: Integer;
  const Values: TContainer): Boolean;
begin
  Selected := 1;
  Result := Item = Values.Value
end;

operator In(
  const Item: Integer;
  const Values: TIntegerSet): Boolean;
begin
  Selected := 2;
  IntegerValues := Values;
  Result := Item = -1
end;

begin
  Container.Value := 9;
  Selected := 0;
  if not (9 in Container) or
     (Selected <> 1) then
    Halt(1);

  IntegerValues := [9];
  Selected := 0;
  if (9 in IntegerValues) or
     (Selected <> 2) then
    Halt(2);

  { Contextual bracket construction is candidate-local. The complete typed
    custom declaration therefore still dominates System's generic fallback. }
  Selected := 0;
  if (9 in [9]) or
     (Selected <> 2) then
    Halt(3);

  { A set type not covered by the custom declaration reaches System's
    `(T, set of T)` fallback and does not call either custom body. }
  CharacterValues := ['A'];
  Selected := 0;
  if not ('A' in CharacterValues) or
     (Selected <> 0) then
    Halt(4)
end.
