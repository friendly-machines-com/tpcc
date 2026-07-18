program PointerEquality;

type
  PInteger = ^Integer;
  TBase = class(TObject);
  TFirst = class(TBase);
  TSecond = class(TBase);

var
  ValueA, ValueB: Integer;
  Raw, RawCopy: Pointer;
  Typed, TypedCopy: PInteger;
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
