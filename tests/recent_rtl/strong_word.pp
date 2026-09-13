program RecentStrongWord;
type TRegister = type Word;
var R: TRegister; C: Cardinal;
procedure Take(R: TRegister);
begin if R <> 123 then Halt(1) end;
begin
  C := 123; R := C; Take(C);
  if R <> 123 then Halt(2);
  {$R-} C := 65537; R := C; {$R+}
  if R <> 1 then Halt(3)
end.
