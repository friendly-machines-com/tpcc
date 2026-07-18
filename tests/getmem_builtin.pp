program GetMemBuiltin;

var
  Characters: PChar;
  Raw: Pointer;
  Status: PtrUInt;

begin
  GetMem(Characters, 8);
  Characters[0] := 'a';
  Characters := ReAllocMem(Characters, 16);
  if Characters[0] <> 'a' then
    Halt(1);
  ReAllocMem(Characters, 32);
  if Characters[0] <> 'a' then
    Halt(2);
  Characters := ReAllocMem(Characters, 4);
  if Characters[0] <> 'a' then
    Halt(3);
  Characters := ReAllocMem(Characters, 0);
  if Characters <> nil then
    Halt(4);

  GetMem(Characters, 8);
  Status := FreeMem(Characters);
  if Status <> 0 then
    Halt(5);

  Raw := nil;
  Raw := ReAllocMem(Raw, 8);
  if Raw = nil then
    Halt(6);
  ReAllocMem(Raw, 0);
  if Raw <> nil then
    Halt(7);

  Raw := GetMem(8);
  FreeMem(Raw, 8)
end.
