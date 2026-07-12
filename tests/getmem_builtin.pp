program GetMemBuiltin;

var
  Characters: PChar;
  Raw: Pointer;
  Status: PtrUInt;

begin
  GetMem(Characters, 8);
  Status := FreeMem(Characters);
  Raw := GetMem(8);
  FreeMem(Raw, 8)
end.
