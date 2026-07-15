program class_static_to_method_rejected;

type
  TBound = procedure of object;
  TFoo = class
    class procedure Run; static;
  end;

class procedure TFoo.Run;
begin
end;

var
  Bound: TBound;
begin
  Bound := @TFoo.Run
end.
