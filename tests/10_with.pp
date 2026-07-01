program p;
type
  TFoo = record
    x: Integer;
  end;
var
  r: TFoo;
begin
  with r do
    x := 42
end.
