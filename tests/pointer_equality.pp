program PointerEquality;

type
  PInteger = ^Integer;
  PAnotherChar = ^Char;
  TBase = class(TObject);
  TFirst = class(TBase);
  TSecond = class(TBase);

var
  ValueA, ValueB: Integer;
  Chars: array[0..2] of Char;
  Raw, RawCopy: Pointer;
  Typed, TypedCopy: PInteger;
  FirstChar, SecondChar, FirstCharCopy: PChar;
  AnotherFirstChar, AnotherSecondChar: PAnotherChar;
  Base: TBase;
  First, FirstCopy: TFirst;
  Second: TSecond;
  Failed: Boolean;

begin
  Failed := False;

  Raw := nil;
  if not (Raw = nil) then
    Failed := True;
  if not (nil = Raw) then
    Failed := True;
  if Raw <> nil then
    Failed := True;

  Raw := @ValueA;
  if Raw = nil then
    Failed := True;
  RawCopy := Raw;
  if not (Raw = RawCopy) then
    Failed := True;
  RawCopy := @ValueB;
  if Raw = RawCopy then
    Failed := True;

  Typed := nil;
  if not (Typed = nil) then
    Failed := True;
  if not (nil = Typed) then
    Failed := True;
  if Typed <> nil then
    Failed := True;

  Typed := @ValueA;
  if Typed = nil then
    Failed := True;
  TypedCopy := Typed;
  if not (Typed = TypedCopy) then
    Failed := True;
  TypedCopy := @ValueB;
  if Typed = TypedCopy then
    Failed := True;

  { FPC treats PChar as a pointer for comparisons. Deliberately put a
    lexicographically greater character at the lower address: FirstChar <
    SecondChar must still be true because it compares addresses. }
  Chars[0] := 'z';
  Chars[1] := 'a';
  Chars[2] := #0;
  FirstChar := @Chars[0];
  SecondChar := @Chars[1];
  FirstCharCopy := FirstChar;
  if not (FirstChar = FirstCharCopy) then
    Failed := True;
  if FirstChar <> FirstCharCopy then
    Failed := True;
  if FirstChar = SecondChar then
    Failed := True;
  if not (FirstChar <> SecondChar) then
    Failed := True;
  if not (FirstChar < SecondChar) then
    Failed := True;
  if not (FirstChar <= FirstCharCopy) then
    Failed := True;
  if not (FirstChar <= SecondChar) then
    Failed := True;
  if not (SecondChar > FirstChar) then
    Failed := True;
  if not (SecondChar >= SecondChar) then
    Failed := True;
  if not (SecondChar >= FirstChar) then
    Failed := True;

  { A separately declared ^Char type reaches the same PChar operator by its
    exact pointee contract; this is not limited to the System alias name. }
  AnotherFirstChar := @Chars[0];
  AnotherSecondChar := @Chars[1];
  if not (AnotherFirstChar < AnotherSecondChar) then
    Failed := True;
  if AnotherFirstChar = AnotherSecondChar then
    Failed := True;

  First := nil;
  if not (First = nil) then
    Failed := True;
  if not (nil = First) then
    Failed := True;
  if First <> nil then
    Failed := True;

  First := TFirst.Create;
  FirstCopy := First;
  Base := First;
  Second := TSecond.Create;

  if not (First = FirstCopy) then
    Failed := True;
  if not (First = Base) then
    Failed := True;
  if First <> Base then
    Failed := True;
  if First = Second then
    Failed := True;

  First.Free;
  Second.Free;

  if Failed then
    Halt(1)
end.
