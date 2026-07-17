program CustomEnumerators;

uses SysUtils;

type
  TEnumerator = class
  private
    FCurrent: Integer;
  public
    constructor Create;
    destructor Destroy; override;
    function MoveNext: Boolean;
    property Current: Integer read FCurrent;
  end;

  TCollection = class
  public
    function GetEnumerator: TEnumerator;
  end;

  TNilCollection = class
  public
    function GetEnumerator: TEnumerator;
  end;

  TRecordEnumerator = record
    CurrentValue: Integer;
    function MoveNext: Boolean;
    property Current: Integer read CurrentValue;
  end;

  TRecordCollection = record
    function GetEnumerator: TRecordEnumerator;
  end;

  TObjectEnumerator = object
    CurrentValue: Integer;
    function MoveNext: Boolean;
    destructor Done;
    property Current: Integer read CurrentValue;
  end;

  TObjectCollection = object
    function GetEnumerator: TObjectEnumerator;
  end;

var
  Destroyed: Integer;
  Collection: TCollection;
  NilCollection: TNilCollection;
  Item: Integer;
  Sum: Integer;
  RecordCollection: TRecordCollection;
  ObjectCollection: TObjectCollection;

constructor TEnumerator.Create;
begin
  FCurrent := 0
end;

destructor TEnumerator.Destroy;
begin
  Destroyed := Destroyed + 1;
  inherited Destroy
end;

function TEnumerator.MoveNext: Boolean;
begin
  FCurrent := FCurrent + 1;
  Result := FCurrent <= 4
end;

function TCollection.GetEnumerator: TEnumerator;
begin
  Result := TEnumerator.Create
end;

function TNilCollection.GetEnumerator: TEnumerator;
begin
  Result := nil
end;

function TRecordEnumerator.MoveNext: Boolean;
begin
  CurrentValue := CurrentValue + 1;
  Result := CurrentValue <= 3
end;

function TRecordCollection.GetEnumerator: TRecordEnumerator;
begin
  Result.CurrentValue := 0
end;

function TObjectEnumerator.MoveNext: Boolean;
begin
  CurrentValue := CurrentValue + 1;
  Result := CurrentValue <= 3
end;

destructor TObjectEnumerator.Done;
begin
  Destroyed := Destroyed + 1
end;

function TObjectCollection.GetEnumerator: TObjectEnumerator;
begin
  Result.CurrentValue := 0
end;

function ExitFromLoop(C: TCollection): Integer;
var
  Value: Integer;
begin
  for Value in C do
  begin
    if Value = 2 then
      Exit(17)
  end;
  Result := 0
end;

procedure RaiseFromLoop(C: TCollection);
var
  Value: Integer;
begin
  for Value in C do
    raise Exception.Create('raised')
end;

begin
  Destroyed := 0;
  Collection := TCollection.Create;
  NilCollection := TNilCollection.Create;

  Sum := 0;
  for Item in Collection do
  begin
    if Item = 2 then
      Continue;
    Sum := Sum + Item
  end;
  if Sum <> 8 then Halt(1);
  if Destroyed <> 1 then Halt(2);

  for Item in Collection do
    Break;
  if Destroyed <> 2 then Halt(3);

  if ExitFromLoop(Collection) <> 17 then Halt(4);
  if Destroyed <> 3 then Halt(5);

  for Item in NilCollection do
    Halt(6);
  if Destroyed <> 3 then Halt(7);

  Sum := 0;
  for Item in RecordCollection do
    Sum := Sum + Item;
  if Sum <> 6 then Halt(8);

  Sum := 0;
  for Item in ObjectCollection do
    Sum := Sum + Item;
  if Sum <> 6 then Halt(9);
  if Destroyed <> 4 then Halt(10);

  try
    RaiseFromLoop(Collection)
  except
    on E: Exception do
      if Length(E.Message) = 0 then
        Halt(11)
  end;
  if Destroyed <> 5 then Halt(12);

  Collection.Free;
  NilCollection.Free
end.
