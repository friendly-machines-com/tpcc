program p;
type
  TFoo = class(TObject)
    x: Integer;
    procedure Bar(y: Integer);
    class function Meta: TClass;
    class function MetaName: shortstring; virtual;
  end;

  TChild = class(TFoo)
    class function MetaName: shortstring; override;
  end;

  TFooClass = class of TFoo;
  TChildClass = class of TChild;

procedure Select(Value: TFooClass); overload;
begin
  WriteLn('base:', Value.ClassName)
end;

procedure Select(Value: TChildClass); overload;
begin
  WriteLn('child:', Value.ClassName)
end;

procedure TFoo.Bar(y: Integer);
begin
  x := y
end;

class function TFoo.Meta: TClass;
begin
  Result := Self
end;

class function TFoo.MetaName: shortstring;
begin
  Result := Self.ClassName
end;

class function TChild.MetaName: shortstring;
begin
  Result := 'child'
end;

var
  c: TClass;
  f: TFoo;
  BaseClass: TFooClass;
  ChildClass: TChildClass;
begin
  c := nil;
  f := TChild.Create;
  WriteLn(f.MetaName);
  Select(TFoo);
  Select(TChild);
  BaseClass := TChild;
  ChildClass := TChildClass(BaseClass);
  WriteLn(ChildClass.ClassName);
  f.Free
end.
