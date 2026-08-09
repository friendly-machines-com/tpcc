program EnvironmentVariableTest;

uses
  SysUtils;

var
  Name, Value: AnsiString;
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
    Halt(12)
end.
