program ClassPointerConversionRejected;

{$interfaces corba}

type
  TClassValue = class
  end;
  TKind = class of TClassValue;
  PInteger = ^Integer;
  TOldValue = object
    Value: Integer;
  end;
  IValue = interface
  end;
  TResult = record
    Value: Integer;
  end;

var
  Instance: TClassValue;
  Kind: TKind;
  TypedPointer: PInteger;
  Raw: Pointer;
  OldValue: TOldValue;
  InterfaceValue: IValue;
  Converted: TResult;

procedure TakeVar(var Value: Pointer);
begin
  Value := Value
end;

procedure TakeOut(out Value: Pointer);
begin
  Value := nil
end;

operator Implicit(Value: Pointer): TResult;
begin
  Value := Value;
  Result.Value := 1
end;

begin
  {$ifdef TEST_TYPED_POINTER}
  TypedPointer := Instance;
  {$endif}
  {$ifdef TEST_CLASSREF_TYPED_POINTER}
  TypedPointer := Kind;
  {$endif}
  {$ifdef TEST_VAR}
  TakeVar(Instance);
  {$endif}
  {$ifdef TEST_OUT}
  TakeOut(Instance);
  {$endif}
  {$ifdef TEST_OLD_OBJECT}
  Raw := OldValue;
  {$endif}
  {$ifdef TEST_INTERFACE}
  Raw := InterfaceValue;
  {$endif}
  {$ifdef TEST_CHAIN}
  Converted := Instance;
  {$endif}
  Raw := nil
end.
