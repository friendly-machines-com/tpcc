program metaclass_lifecycle;

type
  TBase = class(TObject)
  public
    class var Value: Integer;
    class constructor Initialize;
    procedure Initialize;
  end;

  TChild = class(TBase)
  public
    class constructor Initialize;
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

begin
  WriteLn(TChild.Value)
end.
