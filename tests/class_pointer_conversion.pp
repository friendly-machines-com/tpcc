program ClassPointerConversion;

type
  TBase = class
  end;
  TChild = class(TBase)
  end;
  TKind = class of TBase;

var
  Instance: TChild;
  Kind: TKind;
  Raw: Pointer;
  Code: CodePointer;
  Seen: Pointer;

procedure TakePointer(Value: Pointer);
begin
  Seen := Value
end;

procedure TakeConstPointer(const Value: Pointer);
begin
  Seen := Value
end;

function ReturnPointer(Value: TBase): Pointer;
begin
  Result := Value
end;

function Pick(Value: TBase): Integer; overload;
begin
  if Value = Value then
    Result := 1
end;

function Pick(Value: Pointer): Integer; overload;
begin
  if Value = Value then
    Result := 2
end;

begin
  Instance := TChild.Create;

  Raw := Instance;
  if Raw <> Pointer(Instance) then
    Halt(1);

  Code := Instance;
  if Pointer(Code) <> Raw then
    Halt(2);

  TakePointer(Instance);
  if Seen <> Raw then
    Halt(3);

  TakeConstPointer(Instance);
  if Seen <> Raw then
    Halt(4);

  if ReturnPointer(Instance) <> Raw then
    Halt(5);

  Kind := TChild;
  Raw := Kind;
  if Raw <> Pointer(Kind) then
    Halt(6);

  { Erasing the class category is deliberately worse than following the
    ordinary class hierarchy, regardless of hierarchy depth. }
  if Pick(Instance) <> 1 then
    Halt(7);
  if Pick(Raw) <> 2 then
    Halt(8);

  Instance.Free
end.
