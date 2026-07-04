// fail
program p;
type
  TFoo = record
    x: Integer;
    procedure Bar(y: Integer);
  end;
procedure TFoo.Bar(y: Integer);
begin
  x := y
end;
var
  f: TFoo;
begin
  f.Bar(5)
end.
