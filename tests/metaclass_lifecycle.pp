program metaclass_lifecycle;

type
  TBase = class(TObject)
  public
    class var Value: Integer;
    class constructor Initialize;
    class destructor Finalize;
    procedure Initialize;
    procedure Finalize;
  end;

  TChild = class(TBase)
  public
    class constructor Initialize;
    class destructor Finalize;
  end;

class constructor TBase.Initialize;
begin
  TBase.Value := 10;
  WriteLn('base')
end;

class constructor TChild.Initialize;
begin
  TBase.Value := TBase.Value + 1;
  WriteLn('child')
end;

procedure TBase.Initialize;
begin
end;

procedure TBase.Finalize;
begin
end;

class destructor TBase.Finalize;
begin
  WriteLn('base final')
end;

class destructor TChild.Finalize;
begin
  WriteLn('child final')
end;

begin
  WriteLn(TChild.Value)
end.
