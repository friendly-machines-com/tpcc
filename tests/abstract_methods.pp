program AbstractMethods;

type
  IContract = interface
    procedure InterfaceMethod;
  end;

  TBase = class(TObject)
  public
    procedure Missing; virtual; abstract;
    function MissingValue: Integer; virtual; abstract;
    class procedure MissingClass; virtual; abstract;
    procedure Concrete;
  end;

  TChild = class(TBase)
  public
    procedure Missing; override;
    function MissingValue: Integer; override;
    class procedure MissingClass; override;
  end;

  TAbstractConstructor = class(TObject)
  public
    constructor Create; virtual; abstract;
  end;

  TConcreteConstructor = class(TAbstractConstructor)
  public
    constructor Create; override;
  end;

procedure TBase.Concrete;
begin
  WriteLn('concrete')
end;

procedure TChild.Missing;
begin
  WriteLn('implemented')
end;

function TChild.MissingValue: Integer;
begin
  Result := 7
end;

class procedure TChild.MissingClass;
begin
  WriteLn('class implemented')
end;

constructor TConcreteConstructor.Create;
begin
end;

var
  Base: TBase;
  Child: TChild;
  Constructed: TAbstractConstructor;

begin
  Base := TBase.Create;
  Base.Concrete;
  Base.Free;

  Child := TChild.Create;
  Child.Missing;
  if Child.MissingValue <> 7 then
    Halt(1);
  Child.MissingClass;
  Child.Free;

  Constructed := TConcreteConstructor.Create;
  Constructed.Free
end.
