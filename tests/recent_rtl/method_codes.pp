program RecentMethodCodes;
type
  TCounter = class
    Value: Integer;
    procedure AddOne;
    procedure AddTwo;
  end;
  TAction = procedure of object;
procedure TCounter.AddOne;
begin Inc(Value) end;
procedure TCounter.AddTwo;
begin Inc(Value, 2) end;
procedure Dispatch(Obj: TCounter; Index: Integer);
const Codes: array[0..1] of Pointer = (@TCounter.AddOne, @TCounter.AddTwo);
var M: TMethod;
begin
  M.Code := Codes[Index]; M.Data := Obj; TAction(M)()
end;
var A, B: TCounter;
begin
  A := TCounter.Create; B := TCounter.Create;
  A.Value := 10; B.Value := 20;
  Dispatch(A, 0); Dispatch(B, 1); Dispatch(A, 1);
  if (A.Value <> 13) or (B.Value <> 22) then Halt(1);
  A.Free; B.Free
end.
