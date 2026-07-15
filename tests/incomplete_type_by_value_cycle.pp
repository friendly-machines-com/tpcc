program IncompleteTypeByValueCycle;

type
  TRecursive = record
    Value: TRecursive;
  end;

begin
end.
