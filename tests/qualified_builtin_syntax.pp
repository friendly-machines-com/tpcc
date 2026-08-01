program QualifiedBuiltinSyntax;

type
  PInteger = ^Integer;

var
  P: PInteger;
  N: Integer;
  S: ShortString;

begin
  System.Write('qualified ', 7);
  System.WriteLn;

  System.New(P);
  P^ := 42;
  if P^ <> 42 then Halt(1);

  if System.SizeOf(Byte) <> 1 then Halt(2);
  if System.SizeOf([2, 300, -40000]) <>
     3 * System.SizeOf(Integer) then Halt(3);
  if System.Low(Byte) <> 0 then Halt(4);
  if System.High(Byte) <> 255 then Halt(5);
  if System.Low([1, 2, 3]) <> 0 then Halt(6);
  if System.High([1, 2, 3]) <> 2 then Halt(7);

  N := 42;
  System.Str(N:5, S);
  if S <> '   42' then Halt(8);

  System.Dispose(P)
end.
