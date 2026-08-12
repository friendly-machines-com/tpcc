program ordinary_external;

function ForeignValue(Value: Integer): Integer;
  external name '::foreign_provider::p_value';

var
  ResultValue: Integer;

begin
  ResultValue := ForeignValue(21);
end.
