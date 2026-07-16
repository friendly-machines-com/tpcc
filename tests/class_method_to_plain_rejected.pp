program class_method_to_plain_rejected;

type
  TPlain = procedure;
  TFoo = class
    class procedure Run;
  end;

class procedure TFoo.Run;
begin
end;

var
  Plain: TPlain;
begin
  Plain := @TFoo.Run
end.
