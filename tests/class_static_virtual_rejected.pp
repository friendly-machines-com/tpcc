program class_static_virtual_rejected;

type
  TFoo = class
    class procedure Run; static; virtual;
  end;

begin
end.
