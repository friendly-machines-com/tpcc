program old_object_lifecycle;

type
  TBase = object
    Value: Integer;
    constructor Init(NewValue: Integer = 11);
    procedure StaticWho;
    procedure VirtualWho; virtual;
    destructor Done; virtual;
  end;
  PBase = ^TBase;

  TDerived = object(TBase)
    procedure VirtualWho; virtual;
    destructor Done; virtual;
  end;
  PDerived = ^TDerived;

  TFailing = object
    constructor Init;
  end;
  PFailing = ^TFailing;

  TPlain = record
    Value: Integer;
    procedure CopyFrom(const Source: TPlain);
  end;
  PPlain = ^TPlain;

  TValueObject = object
    Value: Integer;
    procedure CopyFrom(const Source: TValueObject);
  end;

  TFailingClass = class(TObject)
    constructor Create;
  end;

var
  FailedFinally: Integer;
  FailedExcept: Integer;
  FailedAfter: Integer;

constructor TBase.Init(NewValue: Integer);
begin
  Value := NewValue;
  StaticWho;
  VirtualWho
end;

procedure TBase.StaticWho;
begin
  WriteLn('base static')
end;

procedure TBase.VirtualWho;
begin
  WriteLn('base virtual')
end;

procedure TDerived.VirtualWho;
begin
  WriteLn('derived virtual')
end;

destructor TBase.Done;
begin
  WriteLn('base done');
  VirtualWho
end;

destructor TDerived.Done;
begin
  WriteLn('derived done');
  inherited Done
end;

constructor TFailing.Init;
begin
  try
    try
      Fail
    finally
      FailedFinally := FailedFinally + 1
    end
  except
    FailedExcept := FailedExcept + 1
  end;
  FailedAfter := FailedAfter + 1
end;

constructor TFailingClass.Create;
begin
  Fail
end;

procedure TPlain.CopyFrom(const Source: TPlain);
begin
  Self := Source
end;

procedure TValueObject.CopyFrom(const Source: TValueObject);
begin
  Self := Source
end;

var
  Derived: PDerived;
  DerivedDefault: PDerived;
  Base: PBase;
  Failing: PFailing;
  NilDerived: PDerived;
  Plain: PPlain;
  PlainFunctional: PPlain;
  PlainSource: TPlain;
  PlainDestination: TPlain;
  ObjectSource: TValueObject;
  ObjectDestination: TValueObject;
  FailingClass: TFailingClass;
begin
  Derived := New(PDerived, Init(7));
  WriteLn(Derived^.Value);

  New(DerivedDefault, Init);
  WriteLn(DerivedDefault^.Value);

  Base := Derived;
  Dispose(Base, Done);
  Dispose(DerivedDefault, Done);

  Failing := New(PFailing, Init);
  if Failing = nil then
    WriteLn(1)
  else
    WriteLn(0);
  WriteLn(FailedFinally);
  WriteLn(FailedExcept);
  WriteLn(FailedAfter);

  FailingClass := TFailingClass.Create;
  if Assigned(FailingClass) then
    WriteLn(0)
  else
    WriteLn(1);

  New(Plain);
  Plain^.Value := 23;
  WriteLn(Plain^.Value);
  Dispose(Plain);

  PlainFunctional := New(PPlain);
  PlainFunctional^.Value := 29;
  WriteLn(PlainFunctional^.Value);
  Dispose(PlainFunctional);

  PlainSource.Value := 31;
  PlainDestination.Value := 0;
  PlainDestination.CopyFrom(PlainSource);
  WriteLn(PlainDestination.Value);

  ObjectSource.Value := 37;
  ObjectDestination.Value := 0;
  ObjectDestination.CopyFrom(ObjectSource);
  WriteLn(ObjectDestination.Value);

  NilDerived := nil;
  Dispose(NilDerived, Done)
end.
