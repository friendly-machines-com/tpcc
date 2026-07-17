program CustomEnumeratorMoveNextRejected;

type
  TEnumerator = record
    Value: Integer;
    function MoveNext: Integer;
    property Current: Integer read Value;
  end;

  TCollection = record
    function GetEnumerator: TEnumerator;
  end;

var
  Collection: TCollection;
  Item: Integer;

function TEnumerator.MoveNext: Integer;
begin
  Result := 0
end;

function TCollection.GetEnumerator: TEnumerator;
begin
  Result.Value := 0
end;

begin
  for Item in Collection do
    Halt(1)
end.
