program RecentStrongWordChecked;
{$R+}
type TRegister = type Word;
var R: TRegister; C: Cardinal;
begin
  C := 65536;
  R := C;
  WriteLn('range check missed');
  Halt(0)
end.
