program AbstractClasses;

type
  TMarked = class abstract (TObject)
  public
    procedure Concrete;
  end;

  TChild = class(TMarked)
  end;

  TMarkedClass = class of TMarked;

procedure TMarked.Concrete;
begin
  WriteLn('concrete')
end;

var
  Exact: TMarked;
  Child: TChild;
  Kind: TMarkedClass;
  ViaClassRef: TMarked;

begin
  Exact := TMarked.Create;
  Exact.Concrete;
  Exact.Free;

  Child := TChild.Create;
  Child.Concrete;
  Child.Free;

  Kind := TMarked;
  ViaClassRef := Kind.Create;
  ViaClassRef.Concrete;
  ViaClassRef.Free
end.
