program WriteBuiltin;

var
  Destination: Text;
  C: Char;
  Value: Extended;

begin
  C := 'Z';
  Value := 2.5;

  Write('A', 12, ' ', C);
  Writeln('!');
  Writeln;
  Writeln(True);
  Writeln(12:4);

  Write(Destination, 'file=', -7);
  Writeln(Destination, ':', Value:0:2)
end.
