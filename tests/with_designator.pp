program WithDesignator;

type
  TItem = record
    Value: Integer;
    Neighbor: Integer;
  end;
  TItemList = array[0..2] of TItem;
  PItemList = ^TItemList;
  TContainer = class
    FItems: PItemList;
    FIndexCalls: Integer;
    function NextIndex: Integer;
    procedure Update;
  end;

var
  Items: TItemList;
  Container: TContainer;

function TContainer.NextIndex: Integer;
begin
  FIndexCalls := FIndexCalls + 1;
  Result := 1
end;

procedure TContainer.Update;
begin
  with FItems^[NextIndex] do
    begin
      Value := 42;
      Neighbor := Value + 1
    end
end;

begin
  Container := TContainer.Create;
  Container.FItems := @Items;
  Container.Update;
  if Container.FIndexCalls <> 1 then
    Halt(1);
  if Items[1].Value <> 42 then
    Halt(2);
  if Items[1].Neighbor <> 43 then
    Halt(3);
  Container.Free
end.
