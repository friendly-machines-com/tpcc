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
    class function ClassSelect(Value: Integer): Integer;
    class function StaticSelect(Value: Integer): Integer; static;
  end;

  TDerived = class(TBase)
  public
    procedure Run;
    function DefaultResult: Integer;
    class function ClassResult: Integer;
    class function StaticResult: Integer; static;
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

class function TBase.ClassSelect(Value: Integer): Integer;
begin
  Result := 300 + Value
end;

class function TBase.StaticSelect(Value: Integer): Integer;
begin
  Result := 400 + Value
end;

procedure TDerived.Run;
begin
  inherited Insert(TDerivedItem.Create);
  if inherited Select(Integer(3)) <> 103 then
    Halt(1)
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

var
  Derived: TDerived;
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
  Derived.Free;
  WriteLn('inherited calls passed')
end.
