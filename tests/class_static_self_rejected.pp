program class_static_self_rejected;

type
  TFoo = class
    class procedure Run; static;
  end;

class procedure TFoo.Run;
begin
  Self.Free
end;

begin
end.
