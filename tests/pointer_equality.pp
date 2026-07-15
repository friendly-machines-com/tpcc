program PointerEquality;

type
  PInteger = ^Integer;

var
  ValueA, ValueB: Integer;
  Raw, RawCopy: Pointer;
  Typed, TypedCopy: PInteger;
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

  if Failed then
    Halt(1)
end.
