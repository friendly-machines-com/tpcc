program ansistring_pointer_cast;

type
  PByte = ^Byte;

var
  S: AnsiString;
  Raw: Pointer;
  Address: PtrInt;
  Character: PByte;
begin
  S := 'abc';
  Raw := Pointer(S);
  Character := PByte(Raw);
  Character^ := 88;
  Address := PtrInt(S);
  Character := PByte(Address + 1);
  Character^ := 89;
  WriteLn(S)
end.
