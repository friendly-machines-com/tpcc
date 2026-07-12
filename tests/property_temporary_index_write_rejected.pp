program property_temporary_index_write_rejected;

type
  TValues = array[1..2] of Integer;

  TBox = object
    function GetValues: TValues;
    property Values: TValues read GetValues;
  end;

var
  Box: TBox;

function TBox.GetValues: TValues;
begin
end;

begin
  Box.Values[1] := 2
end.
