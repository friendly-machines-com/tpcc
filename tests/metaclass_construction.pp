program metaclass_construction;

type
  TBase = class(TObject)
  public
    class var AfterCount: Integer;
    Value: Integer;
    constructor Create(NewValue: Integer);
    class function Kind: Integer; virtual;
    procedure AfterConstruction; override;
  end;

  TChild = class(TBase)
  public
    class function Kind: Integer; override;
  end;

constructor TBase.Create(NewValue: Integer);
begin
  Value := NewValue
end;

class function TBase.Kind: Integer;
begin
  Result := 1
end;

procedure TBase.AfterConstruction;
begin
  AfterCount := AfterCount + 1
end;

class function TChild.Kind: Integer;
begin
  Result := 2
end;

var
  C: class of TBase = TChild;
  Instance: TBase;
begin
  Instance := C.Create(7);
  WriteLn(Instance.Value);
  WriteLn(Instance.Kind);
  WriteLn(TBase.AfterCount);
  Instance.Free
end.
