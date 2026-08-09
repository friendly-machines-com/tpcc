program UnixFpSystemTest;

uses
  Unix;

var
  Status: LongInt;

begin
  Status := Unix.FpSystem('');
  if Status <> 1 then
    Halt(1);

  Status := Unix.FpSystem('exit 7');
  if Status <> (7 shl 8) then
    Halt(2);

  Status :=
    Unix.FpSystem(
      'test "$TPCC_FPSYSTEM_INHERITED" = inherited');
  if Status <> 0 then
    Halt(3);

  Status := Unix.FpSystem('exit 127');
  if Status <> (127 shl 8) then
    Halt(4);

  Status := Unix.FpSystem('kill -TERM $$');
  if Status <> 15 then
    Halt(5)
end.
