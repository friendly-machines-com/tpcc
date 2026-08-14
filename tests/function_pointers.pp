program function_pointers;

type
  TPlainProcedure = procedure(Value: Integer);
  TPlainFunction = function(Value: Integer): Integer;
  TNoArgFunction = function: Integer;
  TNoArgBoolean = function: Boolean;
  TMutation = procedure(var Value: Integer; const Delta: Integer);
  TBoundProcedure = procedure(Value: Integer) of object;
  TBoundFunction = function(Value: Integer): Integer of object;
  TObjectCallback = procedure(Data: TObject; Arg: Pointer);
  TPointerCallback = procedure(Data, Arg: Pointer);
  TObjectMethodCallback =
    procedure(Data: TObject; Arg: Pointer) of object;
  TPointerMethodCallback =
    procedure(Data, Arg: Pointer) of object;

  TBase = class
    Factor: Integer;
    destructor Destroy; virtual;
    procedure Accumulate(Value: Integer); virtual;
    function Scale(Value: Integer): Integer; virtual;
    procedure CaptureObject(Data: TObject; Arg: Pointer);
  end;

type
  TChild = class(TBase)
    ChildFactor: Integer;
    procedure Accumulate(Value: Integer); override;
    function Scale(Value: Integer): Integer; override;
  end;

var
  PlainProcedure: TPlainProcedure;
  PlainProcedureCopy: TPlainProcedure;
  PlainFunction: TPlainFunction;
  NoArgFunction: TNoArgFunction;
  NoArgFunctionCopy: TNoArgFunction;
  NoArgBoolean: TNoArgBoolean;
  Mutation: TMutation;
  BoundProcedure: TBoundProcedure;
  BoundProcedureCopy: TBoundProcedure;
  BoundFunction: TBoundFunction;
  ObjectCallback: TObjectCallback;
  PointerCallback: TPointerCallback;
  ObjectMethodCallback: TObjectMethodCallback;
  PointerMethodCallback: TPointerMethodCallback;
  Receiver: TBase;
  OtherReceiver: TBase;
  RawMethod: TMethod;
  ResultValue: Integer;
  FunctionResult: Integer;
  MutationResult: Integer;
  MethodFunctionResult: Integer;
  PlainCodeEqual: Boolean;
  MethodCodeEqual: Boolean;
  AutoCallEqual: Boolean;
  AutoCallValueArgument: Integer;
  AutoCallRoutineArgument: Integer;
  PlainNil: Boolean;
  PlainNilEqual: Boolean;
  MethodEqual: Boolean;
  MethodNil: Boolean;
  MethodNilEqual: Boolean;
  RawCode: Pointer;
  RawData: Pointer;
  GlobalCode: Pointer;
  GlobalCodeFromValue: Pointer;
  MethodCode: Pointer;
  StaticPointerCastResult: Pointer;
  MethodPointerCastResult: Pointer;

procedure SetResult(Value: Integer); overload;
begin
  ResultValue := Value
end;

procedure SetResult(Value: QWord); overload;
begin
  ResultValue := Integer(Value)
end;

function DoubleValue(Value: Integer): Integer; overload;
begin
  DoubleValue := Value * 2
end;

function DoubleValue(Value: QWord): Integer; overload;
begin
  DoubleValue := Integer(Value)
end;

function FortyTwo: Integer;
begin
  FortyTwo := 42
end;

function Yes: Boolean;
begin
  Yes := True
end;

procedure AcceptValue(Value: Integer);
begin
  AutoCallValueArgument := Value
end;

procedure AcceptRoutine(Value: TNoArgFunction);
begin
  { The routine-typed formal receives the carrier. Its later use in an
    Integer assignment is the value context which performs the call. }
  AutoCallRoutineArgument := Value
end;

procedure Mutate(var Value: Integer; const Delta: Integer);
begin
  Value := Value + Delta
end;

procedure CaptureObject(Data: TObject; Arg: Pointer);
begin
  if Arg = nil then
    StaticPointerCastResult := Pointer(Data)
  else
    StaticPointerCastResult := Arg
end;

function PointerIdentity(Value: Pointer): Pointer;
begin
  Result := Value
end;

destructor TBase.Destroy;
begin
end;

procedure TBase.Accumulate(Value: Integer);
begin
  ResultValue := Factor + Value
end;

function TBase.Scale(Value: Integer): Integer;
begin
  Scale := Factor * Value
end;

procedure TBase.CaptureObject(Data: TObject; Arg: Pointer);
begin
  if Arg = nil then
    MethodPointerCastResult := Pointer(Data)
  else
    MethodPointerCastResult := Arg
end;

procedure TChild.Accumulate(Value: Integer);
begin
  ResultValue := ChildFactor + Value + 100
end;

function TChild.Scale(Value: Integer): Integer;
begin
  Scale := ChildFactor * Value + 100
end;

begin
  PlainProcedure := @SetResult;
  PlainProcedure(7);
  PlainProcedureCopy := PlainProcedure;
  { `@` projects Code. Direct procedural-value equality is not a language
    operation; the bootstrap source's Hook <> @DefaultHook form has this same
    explicit code-address meaning. }
  PlainCodeEqual := not (PlainProcedure <> @SetResult);
  GlobalCodeFromValue := @PlainProcedure;
  PlainProcedure := nil;
  PlainNilEqual := PlainProcedure = nil;
  PlainNil := not Assigned(PlainProcedure);

  PlainFunction := @DoubleValue;
  FunctionResult := PlainFunction(9);

  NoArgFunction := @FortyTwo;
  NoArgFunctionCopy := NoArgFunction;
  AutoCallEqual := NoArgFunction = NoArgFunctionCopy;
  FunctionResult := FunctionResult + NoArgFunction;
  AcceptValue(NoArgFunction);
  AcceptRoutine(NoArgFunction);
  NoArgBoolean := @Yes;
  if NoArgBoolean then
    FunctionResult := FunctionResult + 1;

  MutationResult := 5;
  Mutation := @Mutate;
  Mutation(MutationResult, 3);

  BoundProcedure := @Receiver.Accumulate;
  BoundProcedure(4);
  BoundProcedureCopy := @OtherReceiver.Accumulate;
  MethodCodeEqual := @BoundProcedure = @BoundProcedureCopy;

  RawMethod := TMethod(BoundProcedure);
  RawCode := RawMethod.Code;
  RawData := RawMethod.Data;
  BoundProcedure := nil;
  MethodNilEqual := BoundProcedure = nil;
  MethodNil := not Assigned(BoundProcedure);
  BoundProcedure := TBoundProcedure(RawMethod);
  TMethod(BoundProcedure).Code := RawMethod.Code;
  BoundProcedure(5);

  TMethod(BoundProcedure).Data := Pointer(OtherReceiver);
  BoundProcedure(6);

  BoundFunction := @Receiver.Scale;
  MethodFunctionResult := BoundFunction(3);

  ObjectCallback := @CaptureObject;
  PointerCallback := TPointerCallback(ObjectCallback);
  PointerCallback(Pointer(Receiver), nil);

  ObjectMethodCallback := @Receiver.CaptureObject;
  PointerMethodCallback :=
    TPointerMethodCallback(ObjectMethodCallback);
  PointerMethodCallback(Pointer(OtherReceiver), nil);

  { Pointer value formals apply the same routine-code context as assignment. }
  GlobalCode := PointerIdentity(@Mutate);
  MethodCode := PointerIdentity(@Receiver.Scale)
end.
