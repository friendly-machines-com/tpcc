program p;
type
  TBase = class
    destructor Destroy; virtual;
  end;
  TDerived = class(TBase)
    destructor Destroy; override;
  end;
destructor TBase.Destroy;
begin
end;
destructor TDerived.Destroy;
begin
  inherited Destroy
end;
var
  d: TDerived;
begin
  d := TDerived.Create;
  d.Free
end.
