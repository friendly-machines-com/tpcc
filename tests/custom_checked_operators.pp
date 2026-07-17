program CustomCheckedOperators;

type
  TNumber = record
    Value: Integer;
  end;
  TLegacy = record
    Value: Integer;
  end;

var
  Selected: Integer;
  A, B, C: TNumber;
  L, M, N: TLegacy;

operator UncheckedAdd(Left, Right: TNumber): TNumber;
begin
  Selected := 1;
  Result.Value := Left.Value + Right.Value
end;

operator Add(Left, Right: TNumber): TNumber;
begin
  Selected := 2;
  Result.Value := Left.Value + Right.Value
end;

operator +(Left, Right: TLegacy): TLegacy;
begin
  Selected := Selected + 1;
  Result.Value := Left.Value + Right.Value
end;

function Add(Value: Integer): Integer;
begin
  Result := Value + 10
end;

begin
  A.Value := 20;
  B.Value := 22;

  {$Q-}
  Selected := 0;
  C := A + B;
  if Selected <> 1 then
    Halt(1);
  if C.Value <> 42 then
    Halt(2);

  {$Q+}
  Selected := 0;
  C := A + B;
  if Selected <> 2 then
    Halt(3);
  if C.Value <> 42 then
    Halt(4);
  if Add(32) <> 42 then
    Halt(5);

  L.Value := 19;
  M.Value := 23;
  {$Q-}
  Selected := 0;
  N := L + M;
  if Selected <> 1 then
    Halt(6);
  if N.Value <> 42 then
    Halt(7);

  {$Q+}
  N := L + M;
  if Selected <> 2 then
    Halt(8);
  if N.Value <> 42 then
    Halt(9)
end.
