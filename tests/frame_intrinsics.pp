program FrameIntrinsics;

var
  Frame: Pointer;
  CallerAddress: CodePointer;
  CallerFrame: Pointer;

procedure Probe;
begin
  Frame := Get_Frame;
  if Frame = nil then
    Halt(1);

  CallerAddress := Get_Caller_Addr(Frame);
  if CallerAddress = nil then
    Halt(2);

  CallerFrame := Get_Caller_Frame(Frame);
  if CallerFrame = nil then
    Halt(3);

  CallerAddress := Get_Caller_Addr(Frame, nil);
  CallerFrame := Get_Caller_Frame(Frame, nil)
end;

begin
  Probe
end.
