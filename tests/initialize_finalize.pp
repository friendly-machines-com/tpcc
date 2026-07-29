program InitializeFinalize;

type
  TNested = record
    Marker: Integer;
    Text: AnsiString;
  end;

  TManaged = record
    Marker: Integer;
    Text: AnsiString;
    Nested: TNested;
    Items: array[0..1] of AnsiString;
  end;

var
  Source: TManaged;
  Copy: TManaged;
begin
  Source.Marker := 17;
  Source.Text := 'outer';
  Source.Nested.Marker := 23;
  Source.Nested.Text := 'inner';
  Source.Items[0] := 'zero';
  Source.Items[1] := 'one';

  { Model the FPC GetCopy idiom: Move creates shallow, non-owning copies of
    every managed handle. Initialize must discard those copied handles without
    releasing them, while leaving the ordinary record bytes untouched. }
  Move(Source, Copy, SizeOf(Source));
  Initialize(Copy);
  if Copy.Marker <> 17 then
    Halt(1);
  if Copy.Nested.Marker <> 23 then
    Halt(2);
  if Length(Copy.Text) <> 0 then
    Halt(3);
  if Length(Copy.Nested.Text) <> 0 then
    Halt(4);
  if Length(Copy.Items[0]) <> 0 then
    Halt(5);
  if Length(Copy.Items[1]) <> 0 then
    Halt(6);

  Copy.Text := Source.Text;
  Copy.Nested.Text := Source.Nested.Text;
  Copy.Items[0] := Source.Items[0];
  Copy.Items[1] := Source.Items[1];

  Finalize(Copy);
  if Copy.Marker <> 17 then
    Halt(7);
  if Copy.Nested.Marker <> 23 then
    Halt(8);
  if Length(Copy.Text) <> 0 then
    Halt(9);
  if Length(Copy.Nested.Text) <> 0 then
    Halt(10);
  if Length(Copy.Items[0]) <> 0 then
    Halt(11);
  if Length(Copy.Items[1]) <> 0 then
    Halt(12);

  { Finalized carriers remain empty and safe for the backend's later automatic
    C++ teardown. Repeated Finalize therefore releases nothing a second time. }
  Finalize(Copy);
  if Source.Text <> 'outer' then
    Halt(13);
  if Source.Nested.Text <> 'inner' then
    Halt(14);
  if Source.Items[0] <> 'zero' then
    Halt(15);
  if Source.Items[1] <> 'one' then
    Halt(16);

  WriteLn('Initialize/Finalize passed')
end.
