program IncompleteTypeResolution;

type
  TFirst = record
    Next: ^TSecond;
  end;
  TSecond = record
    Value: Integer;
  end;

  PIndex = ^TIndex;
  TIndexed = record
    function GetItem(I: TIndex): Integer;
    property Items[I: TIndex]: Integer read GetItem; default;
  end;
  TIndex = Integer;

  PScalar = ^TScalar;
  TValue = record
    X: TScalar;
  end;
  TScalar = Integer;
  TConstants = record
    const Three = 3;
    const Four = Three + 1;
    const Seven: TValue = (X: 7);
  end;

  TImplicitClassRef = class of TImplicitClass;
  TImplicitClass = class
  end;

function TIndexed.GetItem(I: TIndex): Integer;
begin
  GetItem := I + 10
end;

procedure TestLocalInitializedStorage;
type
  TLocal = record
    const Value: Integer = 11;
  end;
var
  { Static aggregate access does not evaluate Local. Pascal permits the
    consequently unused variable, so generated C++ must remain warning-clean. }
  Local: TLocal;
begin
  if Local.Value <> 11 then
    Halt(4);
  Local.Value := 12;
  if Local.Value <> 12 then
    Halt(5)
end;

var
  First: TFirst;
  Indexed: TIndexed;
  Index: Integer;
  Constants: TConstants;
  Value: TValue;
  SevenAddress: ^TValue;
  ImplicitClassRef: TImplicitClassRef;

const
  QualifiedFour = Constants.Three + 1;

begin
  First.Next := nil;
  Index := 2;
  if Indexed[Index] <> 12 then
    Halt(1);
  Value := Constants.Seven;
  if Value.X <> 7 then
    Halt(2);
  SevenAddress := @Constants.Seven;
  Constants.Seven.X := Constants.Three;
  if SevenAddress^.X <> 3 then
    Halt(3);
  if Constants.Four <> 4 then
    Halt(6);
  if QualifiedFour <> 4 then
    Halt(7);
  ImplicitClassRef := nil;
  TestLocalInitializedStorage
end.
