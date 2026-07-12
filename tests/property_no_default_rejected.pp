program property_no_default_rejected;

type
  TBox = object
    function GetItem(I: Integer): Integer;
    property Items[I: Integer]: Integer read GetItem;
  end;

var
  Box: TBox;
  X: Integer;

function TBox.GetItem(I: Integer): Integer;
begin
  GetItem := I
end;

begin
  X := Box[1]
end.
