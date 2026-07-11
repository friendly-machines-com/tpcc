program p;
type
  TFoo = class(TObject)
    x: Integer;
    procedure Bar(y: Integer);
    class function Meta: TClass;
    class function MetaName: shortstring;
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

var
  c: TClass;
begin
  c := nil
end.
