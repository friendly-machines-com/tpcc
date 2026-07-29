program GetMemBuiltin;

type
  PByte = ^Byte;

var
  Characters: PChar;
  Raw: Pointer;
  Zeroed: PByte;
  Status: PtrUInt;
  I: Integer;

begin
  Zeroed := AllocMem(16);
  if Zeroed = nil then
    Halt(1);
  for I := 0 to 15 do
    if Zeroed[I] <> 0 then
      Halt(2);
  Zeroed[0] := 37;
  Zeroed := ReAllocMem(Zeroed, 32);
  if Zeroed[0] <> 37 then
    Halt(3);
  Status := FreeMem(Zeroed);
  if Status <> 0 then
    Halt(4);

  GetMem(Characters, 8);
  Characters[0] := 'a';
  Characters := ReAllocMem(Characters, 16);
  if Characters[0] <> 'a' then
    Halt(5);
  ReAllocMem(Characters, 32);
  if Characters[0] <> 'a' then
    Halt(6);
  Characters := ReAllocMem(Characters, 4);
  if Characters[0] <> 'a' then
    Halt(7);
  Characters := ReAllocMem(Characters, 0);
  if Characters <> nil then
    Halt(8);

  GetMem(Characters, 8);
  Status := FreeMem(Characters);
  if Status <> 0 then
    Halt(9);

  Raw := nil;
  Raw := ReAllocMem(Raw, 8);
  if Raw = nil then
    Halt(10);
  ReAllocMem(Raw, 0);
  if Raw <> nil then
    Halt(11);

  Raw := GetMem(8);
  FreeMem(Raw, 8);

  Raw := AllocMem(0);
  FreeMem(Raw)
end.
