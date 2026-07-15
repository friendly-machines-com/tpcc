program FinalMethods;

type
  TBase = class(TObject)
  public
    procedure DirectFinal; virtual; final;
    procedure InstanceMethod; virtual;
    class procedure ClassMethod; virtual;
  end;

  TFinal = class(TBase)
  public
    procedure InstanceMethod; override; final;
    class procedure ClassMethod; override; final;
  end;

procedure TBase.DirectFinal;
begin
end;

procedure TBase.InstanceMethod;
begin
end;

class procedure TBase.ClassMethod;
begin
end;

procedure TFinal.InstanceMethod;
begin
end;

class procedure TFinal.ClassMethod;
begin
end;

begin
end.
