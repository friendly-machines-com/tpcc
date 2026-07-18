program CustomInRejected;

type
  TValue = record
    Value: Integer;
  end;
  TContainer = record
    Value: Integer;
  end;
  TIntegerSet = set of Integer;

var
  Value: TValue;
  Container: TContainer;
  Values: TIntegerSet;
  ResultValue: Boolean;

begin
  {$ifdef REJECT_ITEM}
  ResultValue := Value in Values;
  {$endif}
  {$ifdef REJECT_CONTAINER}
  ResultValue := 1 in Container;
  {$endif}
end.
