program OperatorArgumentMatching;

type
  TBox = record
    Value: Integer;
  end;

var
  Selected: Integer;
  Box: TBox;
  Sum: TBox;

operator :=(Value: Integer): TBox;
begin
  Result.Value := Value
end;

operator +(Left, Right: TBox): TBox;
begin
  Selected := 1;
  Result.Value := Left.Value + Right.Value
end;

operator +(Left: TBox; Right: Integer): TBox;
begin
  Selected := 2;
  Result.Value := Left.Value + Right
end;

begin
  Box.Value := 40;
  Sum := Box + 2;
  if Selected <> 2 then
    Halt(1);
  if Sum.Value <> 42 then
    Halt(2)
end.
