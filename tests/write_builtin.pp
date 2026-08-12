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
  Write(stdout, 'explicit stdout');
  Writeln(stderr, 'explicit stderr');
  Flush(stdout);
  Flush(stderr);

  Write(Destination, 'file=', -7);
  Writeln(Destination, ':', Value:0:2);
  Flush(Destination)
end.
