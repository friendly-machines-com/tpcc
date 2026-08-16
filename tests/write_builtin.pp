program WriteBuiltin;

var
  Destination: Text;
  C: Char;
  Value: Extended;
  PointerValue: PChar;
  PointerBacking: AnsiString;
  CountedText: AnsiString;

begin
  C := 'Z';
  Value := 2.5;
  PointerBacking := 'pointer'#0'ignored';
  PointerValue := PChar(PointerBacking);
  CountedText := 'ab'#0'cd';

  Write('A', 12, ' ', C);
  Writeln('!');
  Writeln;
  Writeln(True);
  Writeln(12:4);
  Writeln(PointerValue:9);
  PointerValue := nil;
  Writeln(PointerValue);
  Write(CountedText);
  Writeln;
  Write(stdout, 'explicit stdout');
  Writeln(stderr, 'explicit stderr');
  Flush(stdout);
  Flush(stderr);

  Write(Destination, 'file=', -7);
  Writeln(Destination, ':', Value:0:2);
  Flush(Destination)
end.
