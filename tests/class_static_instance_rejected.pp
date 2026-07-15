program class_static_instance_rejected;

type
  TFoo = class
    var Value: Integer;
    class function ReadValue: Integer; static;
  end;

class function TFoo.ReadValue: Integer;
begin
  Result := Value
end;

begin
end.
