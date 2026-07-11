program p;

type
  TPackedPair = packed record
    Tag: Byte;
    Value: LongInt;
  end;
  TOrdinaryOuter = record
    Pair: TPackedPair;
  end;

procedure TakeValue(R: TPackedPair);
begin
end;

procedure Change(var R: TPackedPair);
begin
  R.Value := 20
end;

procedure Produce(out R: TPackedPair);
begin
  R.Tag := 3;
  R.Value := 30
end;

function ReadValue(const R: TPackedPair): LongInt;
begin
  Result := R.Value
end;

function MakePair: TPackedPair;
begin
  Result.Tag := 4;
  Result.Value := 40
end;

var
  A, B: TPackedPair;
  Outer: TOrdinaryOuter;
  N: LongInt;

begin
  A.Tag := 1;
  A.Value := 10;
  B := A;
  TakeValue(B);
  Change(B);
  Produce(A);
  N := ReadValue(A);
  B := MakePair;
  Outer.Pair.Value := 50;

  if A.Tag <> 3 then
    begin
      N := 0;
      N := 1 div N
    end;
  if A.Value <> 30 then
    begin
      N := 0;
      N := 1 div N
    end;
  if N <> 30 then
    begin
      N := 0;
      N := 1 div N
    end;
  if B.Tag <> 4 then
    begin
      N := 0;
      N := 1 div N
    end;
  if B.Value <> 40 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Outer.Pair.Value <> 50 then
    begin
      N := 0;
      N := 1 div N
    end
end.
