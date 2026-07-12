program property_method_reference_rejected;

type
  TBox = object
    function GetValue: Integer;
    property Value: Integer read GetValue;
  end;

var
  Box: TBox;
  P: ^Integer;

function TBox.GetValue: Integer;
begin
  GetValue := 1
end;

begin
  P := @Box.Value
end.
