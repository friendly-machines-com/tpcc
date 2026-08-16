program InheritedCalls;

type
  TBaseItem = class
  end;

  TDerivedItem = class(TBaseItem)
  end;

  TBase = class
  public
    procedure Insert(Item: TBaseItem);
    function Select(Value: Integer): Integer; overload;
    function Select(Value: QWord): Integer; overload;
    function WithDefault(Value: Integer = 7): Integer;
    procedure Relay(Value: Integer; const Delta: Integer;
      var Total: Integer; out Seen: Integer);
    class function ClassSelect(Value: Integer): Integer;
    class function StaticSelect(Value: Integer): Integer; static;
  end;

  TDerived = class(TBase)
  public
    constructor Create;
    procedure Run;
    procedure Relay(Value: Integer; const Delta: Integer;
      var Total: Integer; out Seen: Integer);
    function DefaultResult: Integer;
    class function ClassResult: Integer;
    class function StaticResult: Integer; static;
  end;

  TConstructorBase = class
  public
    Stored: Integer;
    constructor Create(Value: Integer);
  end;

  TConstructorDerived = class(TConstructorBase)
  public
    constructor Create(Value: Integer);
  end;

var
  Inserted: Boolean;

procedure TBase.Insert(Item: TBaseItem);
begin
  Inserted := Item <> nil;
  Item.Free
end;

function TBase.Select(Value: Integer): Integer;
begin
  Result := 100 + Value
end;

function TBase.Select(Value: QWord): Integer;
begin
  Result := 200 + Value
end;

function TBase.WithDefault(Value: Integer): Integer;
begin
  Result := Value
end;

procedure TBase.Relay(Value: Integer; const Delta: Integer;
  var Total: Integer; out Seen: Integer);
begin
  Total := Total + Value + Delta;
  Seen := Value
end;

class function TBase.ClassSelect(Value: Integer): Integer;
begin
  Result := 300 + Value
end;

class function TBase.StaticSelect(Value: Integer): Integer;
begin
  Result := 400 + Value
end;

constructor TDerived.Create;
begin
  inherited
end;

procedure TDerived.Run;
begin
  inherited Insert(TDerivedItem.Create);
  if inherited Select(Integer(3)) <> 103 then
    Halt(1)
end;

procedure TDerived.Relay(Value: Integer; const Delta: Integer;
  var Total: Integer; out Seen: Integer);
begin
  inherited;
  Seen := Seen + 1
end;

function TDerived.DefaultResult: Integer;
begin
  Result := inherited WithDefault
end;

class function TDerived.ClassResult: Integer;
begin
  Result := inherited ClassSelect(4)
end;

class function TDerived.StaticResult: Integer;
begin
  Result := inherited StaticSelect(5)
end;

constructor TConstructorBase.Create(Value: Integer);
begin
  Stored := Value
end;

constructor TConstructorDerived.Create(Value: Integer);
begin
  inherited;
  Stored := Stored + 1
end;

var
  Derived: TDerived;
  ConstructorDerived: TConstructorDerived;
  Total: Integer;
  Seen: Integer;
begin
  Derived := TDerived.Create;
  Derived.Run;
  if not Inserted then
    Halt(2);
  if Derived.DefaultResult <> 7 then
    Halt(3);
  if TDerived.ClassResult <> 304 then
    Halt(4);
  if TDerived.StaticResult <> 405 then
    Halt(5);

  Total := 10;
  Seen := 99;
  Derived.Relay(3, 4, Total, Seen);
  if Total <> 17 then
    Halt(6);
  if Seen <> 4 then
    Halt(7);

  ConstructorDerived := TConstructorDerived.Create(41);
  if ConstructorDerived.Stored <> 42 then
    Halt(8);
  ConstructorDerived.Free;

  Derived.Free;
  WriteLn('inherited calls passed')
end.
