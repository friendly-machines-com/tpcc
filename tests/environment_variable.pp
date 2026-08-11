program EnvironmentVariableTest;

uses
  SysUtils, BaseUnix;

var
  Name, Value: AnsiString;
  EnvironmentPointer: PChar;
  I: SizeInt;

begin
  if GetEnvironmentVariable('TPCC_ENV_PRESENT') <>
      'alpha beta:gamma' then
    Halt(1);

  Name := 'TPCC_ENV_PRESENT';
  if GetEnvironmentVariable(Name) <> 'alpha beta:gamma' then
    Halt(2);

  if GetEnvironmentVariable('TPCC_ENV_MISSING') <> '' then
    Halt(3);
  if GetEnvironmentVariable('TPCC_ENV_EMPTY') <> '' then
    Halt(4);
  if GetEnvironmentVariable('tpcc_env_present') <> '' then
    Halt(5);
  if GetEnvironmentVariable('') <> '' then
    Halt(6);
  if GetEnvironmentVariable('TPCC_ENV_PRESENT=ignored') <> '' then
    Halt(7);

  Value := GetEnvironmentVariable('TPCC_ENV_LONG');
  if Length(Value) <> 400 then
    Halt(8);
  for I := 1 to Length(Value) do
    if Value[I] <> 'x' then
      Halt(9);

  Value := GetEnvironmentVariable('TPCC_ENV_BYTES');
  if Length(Value) <> 2 then
    Halt(10);
  if Ord(Value[1]) <> 128 then
    Halt(11);
  if Ord(Value[2]) <> 255 then
    Halt(12);

  { BaseUnix returns borrowed environment storage, while assigning that PChar
    to AnsiString performs the ordinary copying conversion. }
  Name := 'TPCC_ENV_PRESENT';
  EnvironmentPointer :=
    BaseUnix.FpGetEnv(PChar(Pointer(Name)));
  if EnvironmentPointer = nil then
    Halt(13);
  Value := EnvironmentPointer;
  if Value <> 'alpha beta:gamma' then
    Halt(14);

  Name := 'TPCC_ENV_EMPTY';
  EnvironmentPointer :=
    BaseUnix.FpGetEnv(PChar(Pointer(Name)));
  if EnvironmentPointer = nil then
    Halt(15);
  if EnvironmentPointer^ <> #0 then
    Halt(16);

  Name := 'TPCC_ENV_MISSING';
  if BaseUnix.FpGetEnv(PChar(Pointer(Name))) <> nil then
    Halt(17);
  if BaseUnix.FpGetEnv(nil) <> nil then
    Halt(18)
end.
