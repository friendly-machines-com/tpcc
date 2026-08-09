program ManagedTypesAndIteration;

type
  TIntArray = array of Integer;
  TStringArray = array of ShortString;
  TFixed = array[3..4] of Integer;
  TIntSet = set of Byte;
  TWideSet = set of Integer;
  TSmall = 2..4;
  TChoice = (First, Second, Third);

var
  A, B: TIntArray;
  Texts: TStringArray;
  F: TFixed;
  S: TIntSet;
  WideSet: TWideSet;
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

procedure CheckWidened(const X: array of Integer);
begin
  if Length(X) <> 3 then Halt(30);
  if (X[0] <> 2) or (X[1] <> 300) or
     (X[2] <> -40000) then Halt(31)
end;

procedure CheckText(const X: array of ShortString);
begin
  if Length(X) <> 2 then Halt(32);
  if (X[0] <> 'ab') or (X[1] <> 'c') then Halt(33)
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
  CheckWidened([2, 300, -40000]);
  A := [2, 300, -40000];
  CheckWidened(A);
  Texts := ['ab', 'c'];
  CheckText(Texts);
  CheckText(['ab', 'c']);
  if SizeOf([2, 300, -40000]) <>
     3 * SizeOf(Integer) then Halt(34);
  if SizeOf([-40000, 300, 2]) <>
     3 * SizeOf(Integer) then Halt(35);
  if SizeOf([255, -1]) <>
     2 * SizeOf(SmallInt) then Halt(36);
  if SizeOf([-1, 255]) <>
     2 * SizeOf(SmallInt) then Halt(37);
  if Length([1, 2, 3]) <> 3 then Halt(40);
  if Low([1, 2, 3]) <> 0 then Halt(41);
  if High([1, 2, 3]) <> 2 then Halt(42);
  WideSet := [2, 300, -40000];
  if not (2 in WideSet) or
     not (300 in WideSet) or
     not (-40000 in WideSet) then Halt(38);
  if not (300 in ([2, 300, -40000] + [])) then
    Halt(39);
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
  LongText[1000] := 'z';
  if LongText[1000] <> 'z' then Halt(11);
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
