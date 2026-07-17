program RoutineValueOverloadCategories;

type
  TBoundCallback = procedure(
    Data, Argument: Pointer) of object;
  TPlainCallback = procedure(
    Data, Argument: Pointer);

  THandler = class
    procedure BoundCallback(
      Data, Argument: Pointer);
  end;

  TDispatcher = class
    procedure ForEachCall(
      Callback: TBoundCallback;
      Argument: Pointer);
    procedure ForEachCall(
      Callback: TPlainCallback;
      Argument: Pointer);
  end;

var
  Dispatcher: TDispatcher;
  Handler: THandler;
  Selected: Integer;

procedure PlainCallback(
  Data, Argument: Pointer);
begin
  if Data = Argument then
    Selected := Selected
end;

procedure THandler.BoundCallback(
  Data, Argument: Pointer);
begin
  if Data = Argument then
    Selected := Selected
end;

procedure TDispatcher.ForEachCall(
  Callback: TBoundCallback;
  Argument: Pointer);
begin
  Selected := 1;
  Callback(Argument, Argument)
end;

procedure TDispatcher.ForEachCall(
  Callback: TPlainCallback;
  Argument: Pointer);
begin
  Selected := 2;
  Callback(Argument, Argument)
end;

begin
  Dispatcher := TDispatcher.Create;
  Handler := THandler.Create;
  Selected := 0;
  Dispatcher.ForEachCall(
    @PlainCallback, nil);
  if Selected <> 2 then
    Halt(1);

  Selected := 0;
  Dispatcher.ForEachCall(
    @Handler.BoundCallback, nil);
  if Selected <> 1 then
    Halt(2);
  Handler.Free;
  Dispatcher.Free
end.
