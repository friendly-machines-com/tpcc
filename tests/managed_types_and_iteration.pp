program ManagedTypesAndIteration;

type
  TIntArray = array of Integer;
  TFixed = array[3..4] of Integer;
  TIntSet = set of Byte;
  TSmall = 2..4;
  TChoice = (First, Second, Third);

var
  A, B: TIntArray;
  F: TFixed;
  S: TIntSet;
  ShortText: ShortString;
  LongText: AnsiString;
  I, Count, ResultCode: Integer;
  Sum: Int64;
  Ch: Char;
  Choice: TChoice;

procedure ReadConst(const X: array of Integer);
begin
  if Low(X) <> 0 then Halt(20);
  if High(X) <> Length(X) - 1 then Halt(21)
end;

procedure ChangeValue(X: array of Integer);
begin
  if Length(X) > 0 then X[0] := 99
end;

procedure ChangeVar(var X: array of Integer);
begin
  if Length(X) > 0 then X[0] := 77
end;

procedure ClearOut(out X: array of Integer);
begin
  if Length(X) < 0 then Halt(22)
end;

procedure AddOpen(const X: array of Integer);
begin
  for I in X do
    Sum := Sum + I
end;

procedure SelectBracket(X: TIntSet); overload;
begin
  if 1 in X then ResultCode := 1
end;

procedure SelectBracket(X: array of Integer); overload;
begin
  if Length(X) > 0 then ResultCode := 2
end;

begin
  A := [10, 20];
  B := A;
  B[0] := 11;
  if A[0] <> 11 then Halt(1);

  SetLength(B, 2);
  B[0] := 12;
  if A[0] <> 11 then Halt(2);

  F[3] := 30;
  F[4] := 40;
  ReadConst(F);
  ReadConst([50, 60]);
  ChangeValue(F);
  if F[3] <> 30 then Halt(3);
  ChangeVar(F);
  if F[3] <> 77 then Halt(4);
  ClearOut(F);
  if (F[3] <> 0) or (F[4] <> 0) then Halt(5);

  ResultCode := 0;
  SelectBracket([1]);
  if ResultCode <> 1 then Halt(6);

  A := [1, 2, 3];
  Sum := 0;
  Count := 0;
  for I in A do
  begin
    Sum := Sum + I;
    Count := Count + 1;
    if Count = 1 then SetLength(A, 1)
  end;
  if (Sum <> 6) or (Count <> 3) then Halt(7);

  F[3] := 4;
  F[4] := 5;
  Sum := 0;
  for I in F do Sum := Sum + I;
  if Sum <> 9 then Halt(8);

  S := [1, 3..4, 3];
  Sum := 0;
  for I in S do Sum := Sum + I;
  if Sum <> 8 then Halt(9);

  ShortText := 'ab';
  Sum := 0;
  for Ch in ShortText do Sum := Sum + Ord(Ch);
  if Sum <> Ord('a') + Ord('b') then Halt(10);

  LongText := 'cd';
  SetLength(LongText, 1000);
  if Length(LongText) <> 1000 then Halt(11);
  SetLength(LongText, 2);
  Sum := 0;
  for Ch in LongText do Sum := Sum + Ord(Ch);
  if Sum <> Ord('c') + Ord('d') then Halt(12);

  Count := 0;
  for Choice in TChoice do Count := Count + 1;
  if Count <> 3 then Halt(13);

  Sum := 0;
  for I in TSmall do Sum := Sum + I;
  if Sum <> 9 then Halt(14);

  Sum := 0;
  AddOpen([6, 7]);
  if Sum <> 13 then Halt(15);

  Count := 0;
  for I in [] do Count := Count + 1;
  if Count <> 0 then Halt(16)
end.
