program ExecuteProcessTest;

uses
  SysUtils;

var
  Arguments: array[0..4] of AnsiString;
  ChildPath: AnsiString;
  Caught: Boolean;
  CaughtCode: Integer;

procedure Remember(E: EOSError);
begin
  Caught := True;
  CaughtCode := E.ErrorCode
end;

procedure ExpectError(const CommandLine: AnsiString;
  ExpectedCode, FailureCode: Integer);
begin
  Caught := False;
  CaughtCode := 0;
  try
    ExecuteProcess(ChildPath, CommandLine);
    Halt(FailureCode)
  except
    on E: EOSError do
      Remember(E)
  end;
  if not Caught then
    Halt(FailureCode + 1);
  if CaughtCode <> ExpectedCode then
    Halt(FailureCode + 2)
end;

begin
  ChildPath :=
    GetEnvironmentVariable('TPCC_EXECUTE_PROCESS_CHILD');
  if ChildPath = '' then
    Halt(1);

  if ExecuteProcess(ChildPath,
      'string alpha "two words" "" omega') <> 0 then
    Halt(2);
  if ExecuteProcess(ChildPath, '') <> 0 then
    Halt(3);
  if ExecuteProcess(ChildPath, 'environment',
      [ExecInheritsHandles]) <> 0 then
    Halt(4);
  if ExecuteProcess(ChildPath, 'exit 23') <> 23 then
    Halt(5);

  Arguments[0] := 'array';
  Arguments[1] := '';
  Arguments[2] := 'two words';
  Arguments[3] := '"quote"';
  Arguments[4] := 'back\slash';
  if ExecuteProcess(ChildPath, Arguments,
      [ExecInheritsHandles]) <> 0 then
    Halt(6);

  ExpectError('exit 127', 127, 10);
  ExpectError('signal', -15, 20);

  Caught := False;
  CaughtCode := 0;
  try
    ExecuteProcess(
      '/tpcc/execute-process/does-not-exist', '');
    Halt(30)
  except
    on E: EOSError do
      Remember(E)
  end;
  if not Caught then
    Halt(31);
  if CaughtCode <> 127 then
    Halt(32)
end.
