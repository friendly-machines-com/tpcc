program property_write_only_read_rejected;

type
  TBox = object
    procedure SetValue(Value: Integer);
    property Value: Integer write SetValue;
  end;

var
  Box: TBox;
  X: Integer;

procedure TBox.SetValue(Value: Integer);
begin
end;

begin
  X := Box.Value
end.
