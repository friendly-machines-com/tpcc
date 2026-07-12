program CopyBuiltin;

var
  Source: ShortString;
  Middle: ShortString;
  Tail: ShortString;
  Missing: ShortString;
  Embedded: ShortString;
  One: ShortString;
  Trimmed: ShortString;

begin
  Source := 'abcdef';
  Middle := Copy(Source, 2, 3);
  Tail := Copy(Source, 5, 100);
  Missing := Copy(Source, 20, 3);
  Embedded := Copy('a'#0'b', 2, 2);
  One := Copy('x', 1, 1);

  Source := #9'  abc';
  Trimmed := Copy(Source, 4, Length(Source) - 3)
end.
